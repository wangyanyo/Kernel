#include "kernel/sched.h"
#include "kernel/config.h"
#include "kernel/status.h"
#include "kernel/kheap.h"
#include "kernel/file.h"
#include "kernel/string.h"
#include "kernel/elfloader.h"
#include "kernel/print.h"
#include "kernel/kernel.h"

static struct task_struct *processes[KERNEL_MAX_PROCESSES] = {};

struct task_struct *find_task_by_pid(pid_t nr)
{
	if (nr < 0 || nr >= KERNEL_MAX_PROCESSES)
		return NULL;
	
	return processes[nr];
}

static void process_init(struct task_struct *process)
{
	memset(process, 0x00, sizeof(struct task_struct));
}

static int process_load_bin(char *filename, struct task_struct *process)
{
	int res = 0;
	int fd;
	void *program_data_ptr = NULL;
	struct file_stat stat;

	fd = fopen(filename, "r");
	if (!fd) {
		res = -EIO;
		goto out;
	}

	res = fstat(fd, &stat);
	if (res != KERNEL_ALL_OK)
		goto out;

	program_data_ptr = kzalloc(stat.filesize);
	if (!program_data_ptr) {
		res = -ENOMEM;
		goto out;
	}

	if (fread(program_data_ptr, stat.filesize, 1, fd) != 1) {
		res = -EIO;
		goto out;
	}

	process->filetype = PROCESS_FILETYPE_BINARY;
	process->ptr = program_data_ptr;
	process->size = stat.filesize;

out:
	if (res < 0)
		kfree(program_data_ptr);

	fclose(fd);
	return res;
}


static int process_load_elf(char *filename, struct task_struct *process)
{
	int res = 0;
	struct elf_file *elf_file = NULL;

	res = elf_load(filename, &elf_file);
	if (res < 0)
		return res;
	
	process->elf_file = elf_file;
	process->filetype = PROCESS_FILETYPE_ELF;

	return res;
}

static int process_load_data(char *filename, struct task_struct *process)
{
	int res = 0;
	res = process_load_elf(filename, process);
	if (res == -EINFORMATS)
		res = process_load_bin(filename, process);
	return res;
}

static int process_map_elf(struct task_struct *process)
{
	int res = 0;
	struct elf_file *file = process->elf_file;
	struct elf_header *header = elf_header(file);

	for (int i = 0; i < header->e_phnum; ++i) {
		struct elf32_phdr* phdr = elf_program_header(header, i);
		void *phdr_phys_address = elf_phdr_phys_address(file, phdr);
		int flag = PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL;
		if (phdr->p_flags & PF_W)
			flag |= PAGING_IS_WRITEABLE;
		res = paging_map_to(process->task->page_directory, paging_align_lower_address((void *)phdr->p_vaddr),
			phdr_phys_address, paging_align_address(phdr_phys_address + phdr->p_memsz), flag);
		if (IS_ERROR(res))
			break;
	}
	return res;
}

static int process_map_binary(struct task_struct *process)
{
	int res = 0;
	res = paging_map_to(process->task->page_directory, (void *)KERNEL_PROGRAM_VIRTUAL_ADDRESS, process->ptr,
		paging_align_address(process->ptr + process->size), PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL | PAGING_IS_WRITEABLE);
	return res;
}

static int process_map_memory(struct task_struct *process)
{
	int res = 0;

	switch (process->filetype) {
		case PROCESS_FILETYPE_ELF:
			res = process_map_elf(process);
			break;
		case PROCESS_FILETYPE_BINARY:
			res = process_map_binary(process);
			break;
		default:
			panic("process_map_memory: Invalid filetype\n");
	}
	if (res < 0) 
		goto out;

	paging_map_to(process->task->page_directory, (void*)KERNEL_PROGRAM_VIRTUAL_STACK_ADDRESS_END,
		process->stack, paging_align_address(process->stack+KERNEL_USER_PROGRAM_STACK_SIZE),
		PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL | PAGING_IS_WRITEABLE);
out:
	return res;
}

static int process_get_free_slot()
{
	for (int i = 0; i < KERNEL_MAX_PROCESSES; ++i) {
		if (processes[i] == NULL)
			return i;
	}
	return -EISTKN;
}

int process_load_for_slot(char *filename, struct task_struct **process, int process_slot)
{
	int res = 0;
	struct task_struct *_process = NULL;

	/* 检查参数合法性 */
	if (!filename || !process || process_get(process_slot)) {
		res = -EINVAGS;
		goto out;
	}

	/* 创建process */
	_process = kzalloc(sizeof(struct task_struct));
	if (!_process) {
		res = -ENOMEM;
		goto out;
	}
	process_init(_process);

	/* 读取文件内容到内存中，让ptr指向这块内存，size设置为文件大小 */
	res = process_load_data(filename, _process);
	if (res != KERNEL_ALL_OK)
		goto out;

	/* 申请一块栈内存，让stack指向这块内存 */
	_process->stack = kzalloc(KERNEL_USER_PROGRAM_STACK_SIZE);
	if (!_process->stack) {
		res = -ENOMEM;
		goto out;
	}

	/* 创建一个task，并建立映射 */
	_process->task = task_new(_process);
	if (!_process->task) {
		res = -ENOMEM;
		goto out;
	}

	res = process_map_memory(_process);
	if (res < 0)
		goto out;

	/* 设置id，filename */
	_process->id = process_slot;
	strncpy(_process->filename, filename, sizeof(_process->filename));

	/* 收尾 */
	*process = _process;
	processes[process_slot] = _process;
out:
	if (IS_ERROR(res)) {
		process_terminate(_process);
		kfree(_process);
		_process = NULL;
		*process = NULL;
	}

	return res;
}

int process_load(char *filename, struct task_struct **process)
{
	int res = 0;
	int process_slot = process_get_free_slot();
	if (process_slot < 0) {
		res = -EISTKN;
		goto out;
	}
	
	res = process_load_for_slot(filename, process, process_slot);

out:
	return res;
}

int process_load_switch(char *filename, struct task_struct **process)
{
	int res = 0;
	res = process_load(filename, process);
	if (res < 0)
		goto out;

	res = process_switch(*process);

out:
	return res;
}

static int process_find_free_allocations_index(struct task_struct *process)
{
	for (int i = 0; i < KERNEL_MAX_PROGRAM_ALLOCATIONS; ++i) {
		if (process->allocations[i].ptr != NULL)
			continue;
		return i;
	}
	return -ENOMEM;
}

void *process_malloc(struct task_struct *process, size_t size)
{
	int ret = 0;

	void *ptr = kzalloc(size);
	if (!ptr) {
		pr_err("kzalloc fail\n");
		ret = -ENOMEM;
		goto out;
	}
	
	int index = process_find_free_allocations_index(process);
	if (index < 0) {
		pr_err("process_find_free_allocations_index fail\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = paging_map_to(process->task->page_directory, ptr, ptr, paging_align_address(ptr + size),
		PAGING_IS_PRESENT | PAGING_IS_WRITEABLE | PAGING_ACCESS_FROM_ALL);
	if (ret < 0) {
		pr_err("paging_map_to fail\n");
		goto out;
	}

	process->allocations[index].ptr = ptr;
	process->allocations[index].size = size;

out:
	if (ret < 0) {
		kfree(ptr);
		ptr = NULL;
	}
	return ptr;
}

static int process_get_allocation_index_by_addr(struct task_struct *process, void *ptr)
{
	int i;
	for (i = 0; i < KERNEL_MAX_PROGRAM_ALLOCATIONS; ++i)
		if (process->allocations[i].ptr == ptr)
			return i;

	return -1;
}

void process_free(struct task_struct *process, void *ptr)
{
	if (!process || !ptr)
		return;

	int index = process_get_allocation_index_by_addr(process, ptr);
	if (index < 0)
		return;
	
	struct process_allocation *allocation = &process->allocations[index];
	paging_map_to(process->task->page_directory, ptr, allocation->ptr,
		paging_align_address(allocation->ptr + allocation->size), 0x00);
	
	process->allocations[index].ptr = 0x00;
	process->allocations[index].size = 0x00;
	kfree(ptr);
}

void process_get_argument(struct task_struct *process, int *argc, char ***argv)
{
	*argc = process->argument.argc;
	*argv = process->argument.argv;
}

static struct command_argument *process_get_next_command_argument(struct command_argument *command_argument)
{
	return task_virtual_addr_to_physical(process_current()->task, command_argument->next);
}

static int process_count_command_argument(struct command_argument* root_argument)
{
	int res = 0;
	struct command_argument *current = root_argument;
	while (current) {
		res++;
		current = process_get_next_command_argument(current);
	}
	return res;
}

int process_inject_argument(struct task_struct *process, struct command_argument *root_argument)
{
	int res = 0;
	int i = 0;
	int argc;
	char **argv;
	char *argument_str;
	struct command_argument *current = root_argument;

	argc = process_count_command_argument(root_argument);
	if (argc == 0) {
		pr_err("process_count_command_argument fail\n");
		res = -EIO;
		goto out;
	}

	argv = process_malloc(process, sizeof(char *) * argc);
	if (!argv) {
		pr_err("process_malloc fail\n");
		res = -ENOMEM;
		goto out;
	}

	while (current) {
		argument_str = process_malloc(process, sizeof(current->argument) + 1);
		if (!argument_str) {
			pr_err("process_malloc fail\n");
			res = -ENOMEM;
			goto out;
		}

		strncpy(argument_str, current->argument, sizeof(current->argument));
		argv[i++] = argument_str;
		current = process_get_next_command_argument(current);
	}

	process->argument.argc = argc;
	process->argument.argv = argv;

out:
	if (res < 0) {
		for (i = 0; i < argc; ++i)
			process_free(process, argv[i]);
		process_free(process, argv);
	}
	return res;
}

static void process_terminate_allocations(struct task_struct *process)
{
	int i;
	for (i = 0; i < KERNEL_MAX_PROGRAM_ALLOCATIONS; ++i)
		process_free(process, process->allocations[i].ptr);
}

static void process_terminate_elf_data(struct task_struct *process)
{
	elf_close(process->elf_file);
}

static void process_terminate_binary_data(struct task_struct *process)
{
	kfree(process->ptr);
}

static int process_terminate_program_data(struct task_struct *process)
{
	switch(process->filetype) {
		case PROCESS_FILETYPE_ELF:
			process_terminate_elf_data(process);
			break;
		case PROCESS_FILETYPE_BINARY:
			process_terminate_binary_data(process);
			break;
		default:
			return -EINVAGS;
	}

	return 0;
}

static void process_terminate_command_argument(struct task_struct* process)
{
	int i;
	for (i = 0; i < process->argument.argc; ++i)
		kfree(process->argument.argv[i]);
}

static void process_switch_to_any()
{
	int i;
	for (i = 0; i < KERNEL_MAX_PROCESSES; ++i) {
		if (processes[i]) {
			// current_process = processes[i];
			return;
		}
	}

	panic("No processes to switch to\n");
}

static void process_unlink(struct task_struct *process)
{
	processes[process->id] = 0x00;
	
	// if (current_process == process)
	// 	process_switch_to_any();
}

int process_terminate(struct task_struct *process)
{
	int ret = 0;

	if (!process)
		return 0;

	process_terminate_allocations(process);

	ret = process_terminate_program_data(process);
	if (ret < 0)
		goto out;

	process_terminate_command_argument(process);
	
	kfree(process->stack);
	task_free(process->task);
	process_unlink(process);

out:
	return ret;
}

struct task_struct *current_task = 0;

struct task_struct *task_head = 0;
struct task_struct *task_tail = 0;

/* 这里代码冗余，后续用嵌入式链表改造 */
static void task_insert(struct task_struct *task)
{
	if (!task_head) {
		task_head = task;
		task_tail = task;
		current_task = task;
		return;
	}

	task_tail->next = task;
	task->prev = task_tail;
	task->next = NULL;
	task_tail = task;
}

static int task_init(struct task_struct *task, struct task_struct *process)
{
	task->page_directory = paging_new_4gb_chunk(PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);
	if (!task->page_directory)
		return -EIO;

	
	if (process->filetype != PROCESS_FILETYPE_ELF)
		task->registers.ip = elf_header(process->elf_file)->e_entry;
	else
		task->registers.ip = KERNEL_PROGRAM_VIRTUAL_ADDRESS;
	task->registers.ss = USER_DATA_SEGMENT;
	task->registers.cs = USER_CODE_SEGMENT;
	task->registers.esp = KERNEL_PROGRAM_VIRTUAL_STACK_ADDRESS_START;

	task->process = process;

	return 0;
}

/* 这个链表维护起来太麻烦了，以后要换成嵌入式链表 */
static void task_list_remove(struct task_struct *task)
{
	if (task_head == task) {
		task_head = task->next;
	}

	if (task_tail == task) {
		task_tail = task->prev;
	}

	if (current_task == task) {
		current_task = task_get_next(current_task);
	}

	if (task->prev) {
		task->prev->next = task->next;
	}

	if (task->next) {
		task->next->prev = task->prev;
	}
}

struct task_struct *task_new(struct task_struct *process)
{
	int res = 0;
	struct task_struct *task = NULL;

	task = kzalloc(sizeof(struct task_struct));
	if (!task) {
		res = -ENOMEM;
		goto out;
	}

	res = task_init(task, process);
	if (res != KERNEL_ALL_OK)
		goto out;

	task_insert(task);

out:
	if (IS_ERROR(res)) {
		task_free(task);
		return ERROR(task);
	}

	return task;
}

struct task_struct *task_get_next()
{
	if (!current_task || !current_task->next)
		return task_head;
	return current_task->next;
}

int task_free(struct task_struct *task)
{
	if (!task)
		return 0;

	paging_free_4gb(task->page_directory);
	task_list_remove(task);

	kfree(task);
	return 0;
}

int task_switch(struct task_struct *task)
{
	current_task = task;
	paging_switch(task->page_directory);
	return 0;
}

int task_page()
{
	user_registers();
	task_switch(current_task);
	return 0;
}

int task_page_task(struct task_struct *task)
{
	user_registers();
	task_switch(task);
	return 0;
}

int task_run_first_ever_task()
{
	if (!current_task)
		panic("task_run_first_ever_task: No current task exists!\n");

	task_switch(task_head);
	task_return(&task_head->registers);
	return 0;
}

static void task_save_state(struct task_struct *task, struct interrupt_frame *frame)
{
	task->registers.ip = frame->ip;
	task->registers.cs = frame->cs;
	task->registers.flags = frame->flags;
	task->registers.esp = frame->esp;
	task->registers.ss = frame->ss;
	task->registers.eax = frame->eax;
	task->registers.ebp = frame->ebp;
	task->registers.ebx = frame->ebx;
	task->registers.ecx = frame->ecx;
	task->registers.edi = frame->edi;
	task->registers.edx = frame->edx;
	task->registers.esi = frame->esi;
}

void task_current_save_state(struct interrupt_frame *frame)
{
	task_save_state(current, frame);
}

int copy_string_from_task(struct task_struct *task, void *virtual, void *phys, int max)
{
	int res = 0;

	if (!task || !virtual || !phys) {
		res = -EINVAGS;
		goto out;
	}

	if (max < 0 || max > PAGING_PAGE_SIZE) {
		res = -EINVAGS;
		goto out;
	}

	void *tmp = kzalloc(max);
	if (!tmp) {
		res = -ENOMEM;
		goto out;
	}

	uint32_t old_entry = paging_get(task->page_directory, virtual);
	res = paging_map(task->page_directory, tmp, tmp, PAGING_IS_WRITEABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);
	if (res != KERNEL_ALL_OK)
		goto free_out;
	paging_switch(task->page_directory);
	strncpy(tmp, virtual, max);
	kernel_page();

	res = paging_set(task->page_directory->directory_entry, tmp, old_entry);
	if (res != KERNEL_ALL_OK)
		goto free_out;

	strncpy(phys, tmp, max);

free_out:
	kfree(tmp);
out:
	return res;

}

void *task_get_stack_item(struct task_struct *task, int index)
{
	void *result = 0;
	uint32_t *sp_ptr = (uint32_t *)task->registers.esp;
	task_page_task(task);
	result = (void *)sp_ptr[index];
	kernel_page();
	return result;
}

void *task_virtual_addr_to_physical(struct task_struct *task, void *virtual_addr)
{
	return paging_get_physical_address(task->mm->pgd, virtual_addr);
}

/*
 * 这是一个非常简单的调度器；调度器只需要处理两件事，1.当前进程执行多场时间，2.下一个进程如何选择
 * 该调度器会在当前进程发生异常或结束时触发调度，下一个进程直接选择链表中的下一个进程；
 * 由于在process_terminal函数中删除了task，所以现在的current_task就是下一个task
 * 还要执行process_switch(next_task->process)，因为task和process是绑定的
 */
void task_next()
{
	struct task_struct* next_task = current_task;
	if (!next_task)
		panic("No more tasks!\n");
	
	process_switch(next_task);
	task_switch(next_task);
	task_return(&next_task->thread.registers);
}