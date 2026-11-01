#include "asm-x86/idt.h"

#include "kernel/sched.h"
#include "kernel/config.h"
#include "kernel/status.h"
#include "kernel/kheap.h"
#include "kernel/file.h"
#include "kernel/string.h"
#include "kernel/elfloader.h"
#include "kernel/print.h"
#include "kernel/kernel.h"

static struct task_struct *tasks[KERNEL_MAX_TASKS] = {};

struct task_struct *current_task = 0;

// 简易调度器，就绪队列为一条链表；调度实体为task_struct->list
LIST_HEAD(ready_queue);

extern void task_return(struct registers* regs);
extern void restore_general_purpose_registers(struct registers* regs);
extern void user_registers();

static void task_insert(struct task_struct *tsk)
{
	if (list_empty(&ready_queue)) {
		current_task = tsk;
	}
	list_add(&tsk->list, &ready_queue);
}

static struct task_struct *task_get_next()
{
	if (list_empty(&ready_queue)) {
		panic("Kernel panic: no ready_queue exist!");
	}
	return list_first_entry(&ready_queue, struct task_struct, list);
}

static void task_list_remove(struct task_struct *tsk)
{
	list_del_init(&tsk->list);

	if (current_task == tsk) {
		current_task = task_get_next();
	}
}

static int task_paging_init(struct task_struct *tsk)
{
	tsk->page_directory = paging_new_4gb_chunk(PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);
	if (!tsk->page_directory)
		return -EIO;
	return 0;
}

static int task_page_task(struct task_struct *task)
{
	user_registers();
	task_switch(task);
	return 0;
}

static void task_save_state(struct task_struct *tsk, struct interrupt_frame *frame)
{
	tsk->registers.ip = frame->ip;
	tsk->registers.cs = frame->cs;
	tsk->registers.flags = frame->flags;
	tsk->registers.esp = frame->esp;
	tsk->registers.ss = frame->ss;
	tsk->registers.eax = frame->eax;
	tsk->registers.ebp = frame->ebp;
	tsk->registers.ebx = frame->ebx;
	tsk->registers.ecx = frame->ecx;
	tsk->registers.edi = frame->edi;
	tsk->registers.edx = frame->edx;
	tsk->registers.esi = frame->esi;
}

struct task_struct *task_get(int pid)
{
	if (pid < 0 || pid >= KERNEL_MAX_TASKS)
		return NULL;
	
	return tasks[pid];
}

struct task_struct *task_current()
{
	return current_task;
}

int task_switch(struct task_struct *tsk)
{
	current_task = tsk;
	paging_switch(tsk->page_directory);
	return 0;
}

void task_next()
{
	struct task_struct* next_task = current_task;
	if (!next_task)
		panic("No more ready_queue!\n");

	task_switch(next_task);
	task_return(&next_task->registers);
}

int task_page()
{
	user_registers();
	task_switch(current_task);
	return 0;
}

int task_run_first_ever_task()
{
	if (!current_task)
		panic("task_run_first_ever_task: No current task exists!\n");

	task_switch(current_task);
	task_return(&current_task->registers);
	return 0;
}

void task_current_save_state(struct interrupt_frame *frame)
{
	if (!task_current())
		panic("No current task to save");
	
	struct task_struct *tsk = task_current();
	task_save_state(tsk, frame);
}

int copy_string_from_task(struct task_struct *tsk, void *virtual, void *phys, int max)
{
	int res = 0;

	if (!tsk || !virtual || !phys) {
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

	uint32_t old_entry = paging_get(tsk->page_directory, virtual);
	res = paging_map(tsk->page_directory, tmp, tmp, PAGING_IS_WRITEABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);
	if (res != KERNEL_ALL_OK)
		goto free_out;
	paging_switch(tsk->page_directory);
	strncpy(tmp, virtual, max);
	kernel_page();

	res = paging_set(tsk->page_directory->directory_entry, tmp, old_entry);
	if (res != KERNEL_ALL_OK)
		goto free_out;

	strncpy(phys, tmp, max);

free_out:
	kfree(tmp);
out:
	return res;

}

void *task_get_stack_item(struct task_struct *tsk, int index)
{
	void *result = 0;
	uint32_t *sp_ptr = (uint32_t *)tsk->registers.esp;
	task_page_task(tsk);
	result = (void *)sp_ptr[index];
	kernel_page();
	return result;
}

void *task_virtual_addr_to_physical(struct task_struct *tsk, void *virtual_addr)
{
	return paging_get_physical_address(tsk->page_directory, virtual_addr);
}

static void task_init(struct task_struct *tsk)
{
	memset(tsk, 0x00, sizeof(struct task_struct));
}

static int task_load_bin(char *filename, struct task_struct *tsk)
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

	tsk->filetype = TASK_FILETYPE_BINARY;
	tsk->ptr = program_data_ptr;
	tsk->size = stat.filesize;

out:
	if (res < 0)
		kfree(program_data_ptr);

	fclose(fd);
	return res;
}


static int task_load_elf(char *filename, struct task_struct *tsk)
{
	int res = 0;
	struct elf_file *elf_file = NULL;

	res = elf_load(filename, &elf_file);
	if (res < 0)
		return res;
	
	tsk->elf_file = elf_file;
	tsk->filetype = TASK_FILETYPE_ELF;

	return res;
}

static int task_load_data(char *filename, struct task_struct *tsk)
{
	int res = 0;
	res = task_load_elf(filename, tsk);
	if (res == -EINFORMATS)
		res = task_load_bin(filename, tsk);
	return res;
}

static int task_map_elf(struct task_struct *tsk)
{
	int res = 0;
	struct elf_file *file = tsk->elf_file;
	struct elf_header *header = elf_header(file);

	for (int i = 0; i < header->e_phnum; ++i) {
		struct elf32_phdr* phdr = elf_program_header(header, i);
		void *phdr_phys_address = elf_phdr_phys_address(file, phdr);
		int flag = PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL;
		if (phdr->p_flags & PF_W)
			flag |= PAGING_IS_WRITEABLE;
		res = paging_map_to(tsk->page_directory, paging_align_lower_address((void *)phdr->p_vaddr),
			phdr_phys_address, paging_align_address(phdr_phys_address + phdr->p_memsz), flag);
		if (IS_ERROR(res))
			break;
	}
	return res;
}

static int task_map_binary(struct task_struct *tsk)
{
	int res = 0;
	res = paging_map_to(tsk->page_directory, (void *)KERNEL_PROGRAM_VIRTUAL_ADDRESS, tsk->ptr,
		paging_align_address(tsk->ptr + tsk->size), PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL | PAGING_IS_WRITEABLE);
	return res;
}

static int task_map_memory(struct task_struct *tsk)
{
	int res = 0;

	switch (tsk->filetype) {
		case TASK_FILETYPE_ELF:
			res = task_map_elf(tsk);
			break;
		case TASK_FILETYPE_BINARY:
			res = task_map_binary(tsk);
			break;
		default:
			panic("task_map_memory: Invalid filetype\n");
	}
	if (res < 0) 
		goto out;

	paging_map_to(tsk->page_directory, (void*)KERNEL_PROGRAM_VIRTUAL_STACK_ADDRESS_END,
		tsk->stack, paging_align_address(tsk->stack+KERNEL_USER_PROGRAM_STACK_SIZE),
		PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL | PAGING_IS_WRITEABLE);
out:
	return res;
}

static int task_get_free_slot()
{
	for (int i = 0; i < KERNEL_MAX_TASKS; ++i) {
		if (tasks[i] == NULL)
			return i;
	}
	return -EISTKN;
}

static int task_find_free_allocations_index(struct task_struct *tsk)
{
	for (int i = 0; i < KERNEL_MAX_PROGRAM_ALLOCATIONS; ++i) {
		if (tsk->vm_area[i].ptr != NULL)
			continue;
		return i;
	}
	return -ENOMEM;
}

static int task_get_allocation_index_by_addr(struct task_struct *tsk, void *ptr)
{
	int i;
	for (i = 0; i < KERNEL_MAX_PROGRAM_ALLOCATIONS; ++i)
		if (tsk->vm_area[i].ptr == ptr)
			return i;

	return -1;
}

static struct command_argument *task_get_next_command_argument(struct command_argument *command_argument)
{
	return task_virtual_addr_to_physical(task_current(), command_argument->next);
}

static int task_count_command_argument(struct command_argument* root_argument)
{
	int res = 0;
	struct command_argument *current = root_argument;
	while (current) {
		res++;
		current = task_get_next_command_argument(current);
	}
	return res;
}

static void task_terminate_allocations(struct task_struct *tsk)
{
	int i;
	for (i = 0; i < KERNEL_MAX_PROGRAM_ALLOCATIONS; ++i)
		task_free(tsk, tsk->vm_area[i].ptr);
}

static void task_terminate_elf_data(struct task_struct *tsk)
{
	elf_close(tsk->elf_file);
}

static void task_terminate_binary_data(struct task_struct *tsk)
{
	kfree(tsk->ptr);
}

static int task_terminate_program_data(struct task_struct *tsk)
{
	switch(tsk->filetype) {
		case TASK_FILETYPE_ELF:
			task_terminate_elf_data(tsk);
			break;
		case TASK_FILETYPE_BINARY:
			task_terminate_binary_data(tsk);
			break;
		default:
			return -EINVAGS;
	}

	return 0;
}

static void task_terminate_command_argument(struct task_struct* tsk)
{
	int i;
	for (i = 0; i < tsk->argument.argc; ++i)
		kfree(tsk->argument.argv[i]);
}

int task_load(char *filename, struct task_struct **tsk)
{
	int res = 0;
	struct task_struct *_task = NULL;
	int task_slot = task_get_free_slot();
	if (task_slot < 0) {
		res = -EISTKN;
		goto out;
	}
	
	/* 检查参数合法性 */
	if (!filename || !tsk || task_get(task_slot)) {
		res = -EINVAGS;
		goto out;
	}

	/* 创建task */
	_task = kzalloc(sizeof(struct task_struct));
	if (!_task) {
		res = -ENOMEM;
		goto out;
	}

	task_init(_task);
	
	INIT_LIST_HEAD(&_task->list);

	/* 读取文件内容到内存中，让ptr指向这块内存，size设置为文件大小 */
	res = task_load_data(filename, _task);
	if (res != KERNEL_ALL_OK)
		goto out;

	/* 申请一块栈内存，让stack指向这块内存 */
	_task->stack = kzalloc(KERNEL_USER_PROGRAM_STACK_SIZE);
	if (!_task->stack) {
		res = -ENOMEM;
		goto out;
	}

	/* 建立映射 */
	task_paging_init(_task);
	res = task_map_memory(_task);
	if (res < 0)
		goto out;

	/* 设置id，filename */
	_task->id = task_slot;
	strncpy(_task->filename, filename, sizeof(_task->filename));

	/* 设置寄存器 */
	if (_task->filetype != TASK_FILETYPE_ELF)
		_task->registers.ip = elf_header(_task->elf_file)->e_entry;
	else
		_task->registers.ip = KERNEL_PROGRAM_VIRTUAL_ADDRESS;
	_task->registers.ss = USER_DATA_SEGMENT;
	_task->registers.cs = USER_CODE_SEGMENT;
	_task->registers.esp = KERNEL_PROGRAM_VIRTUAL_STACK_ADDRESS_START;

	/* 收尾 */
	*tsk = _task;
	tasks[task_slot] = _task;

	/* 将task插入就绪队列 */
	task_insert(_task);
out:
	if (IS_ERROR(res)) {
		task_terminate(_task);
		kfree(_task);
		_task = NULL;
		*tsk = NULL;
	}

	return res;
}

void *task_malloc(struct task_struct *tsk, size_t size)
{
	int ret = 0;

	void *ptr = kzalloc(size);
	if (!ptr) {
		pr_err("kzalloc fail\n");
		ret = -ENOMEM;
		goto out;
	}
	
	int index = task_find_free_allocations_index(tsk);
	if (index < 0) {
		pr_err("task_find_free_allocations_index fail\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = paging_map_to(tsk->page_directory, ptr, ptr, paging_align_address(ptr + size),
		PAGING_IS_PRESENT | PAGING_IS_WRITEABLE | PAGING_ACCESS_FROM_ALL);
	if (ret < 0) {
		pr_err("paging_map_to fail\n");
		goto out;
	}

	tsk->vm_area[index].ptr = ptr;
	tsk->vm_area[index].size = size;

out:
	if (ret < 0) {
		kfree(ptr);
		ptr = NULL;
	}
	return ptr;
}

void task_free(struct task_struct *tsk, void *ptr)
{
	if (!tsk || !ptr)
		return;

	int index = task_get_allocation_index_by_addr(tsk, ptr);
	if (index < 0)
		return;
	
	struct vm_area_struct *allocation = &tsk->vm_area[index];
	paging_map_to(tsk->page_directory, ptr, allocation->ptr,
		paging_align_address(allocation->ptr + allocation->size), 0x00);
	
	tsk->vm_area[index].ptr = 0x00;
	tsk->vm_area[index].size = 0x00;
	kfree(ptr);
}

void task_get_argument(struct task_struct *tsk, int *argc, char ***argv)
{
	*argc = tsk->argument.argc;
	*argv = tsk->argument.argv;
}

int task_inject_argument(struct task_struct *tsk, struct command_argument *root_argument)
{
	int res = 0;
	int i = 0;
	int argc;
	char **argv;
	char *argument_str;
	struct command_argument *current = root_argument;

	argc = task_count_command_argument(root_argument);
	if (argc == 0) {
		pr_err("task_count_command_argument fail\n");
		res = -EIO;
		goto out;
	}

	argv = task_malloc(tsk, sizeof(char *) * argc);
	if (!argv) {
		pr_err("task_malloc fail\n");
		res = -ENOMEM;
		goto out;
	}

	while (current) {
		argument_str = task_malloc(tsk, sizeof(current->argument) + 1);
		if (!argument_str) {
			pr_err("task_malloc fail\n");
			res = -ENOMEM;
			goto out;
		}

		strncpy(argument_str, current->argument, sizeof(current->argument));
		argv[i++] = argument_str;
		current = task_get_next_command_argument(current);
	}

	tsk->argument.argc = argc;
	tsk->argument.argv = argv;

out:
	if (res < 0) {
		for (i = 0; i < argc; ++i)
			task_free(tsk, argv[i]);
		task_free(tsk, argv);
	}
	return res;
}

int task_terminate(struct task_struct *tsk)
{
	int ret = 0;

	if (!tsk)
		return 0;

	task_terminate_allocations(tsk);

	ret = task_terminate_program_data(tsk);
	if (ret < 0)
		goto out;

	task_terminate_command_argument(tsk);
	
	kfree(tsk->stack);

	paging_free_4gb(tsk->page_directory);

	task_list_remove(tsk);

	tasks[tsk->id] = 0x00;

out:
	return ret;
}