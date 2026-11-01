#include "asm-x86/idt.h"

#include "kernel/task.h"
#include "kernel/kheap.h"
#include "kernel/status.h"
#include "kernel/print.h"
#include "kernel/string.h"
#include "kernel/kernel.h"
#include "kernel/sched.h"
#include "kernel/elfloader.h"

struct thread_info *current_task = 0;

LIST_HEAD(tasks);

static void task_insert(struct thread_info *task)
{
	if (list_empty(&tasks)) {
		current_task = task;
	}
	list_add(&task->list, &tasks);
}

static int task_init(struct thread_info *task, struct task_struct *process)
{
	task->page_directory = paging_new_4gb_chunk(PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);
	if (!task->page_directory)
		return -EIO;

	INIT_LIST_HEAD(&task->list);

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

static void task_list_remove(struct thread_info *task)
{
	list_del_init(&task->list);

	if (current_task == task) {
		current_task = task_get_next();
	}
}

struct thread_info *task_new(struct task_struct *process)
{
	int res = 0;
	struct thread_info *task = NULL;

	task = kzalloc(sizeof(struct thread_info));
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

struct thread_info *task_current()
{
	return current_task;
}

struct thread_info *task_get_next()
{
	if (list_empty(&tasks)) {
		panic("Kernel panic: no tasks exist!");
	}
	return list_first_entry(&tasks, struct thread_info, list);
}

int task_free(struct thread_info *task)
{
	if (!task)
		return 0;

	paging_free_4gb(task->page_directory);
	task_list_remove(task);

	kfree(task);
	return 0;
}

int task_switch(struct thread_info *task)
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

int task_page_task(struct thread_info *task)
{
	user_registers();
	task_switch(task);
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

static void task_save_state(struct thread_info *task, struct interrupt_frame *frame)
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
	if (!task_current())
		panic("No current task to save");
	
	struct thread_info *task = task_current();
	task_save_state(task, frame);
}

int copy_string_from_task(struct thread_info *task, void *virtual, void *phys, int max)
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

void *task_get_stack_item(struct thread_info *task, int index)
{
	void *result = 0;
	uint32_t *sp_ptr = (uint32_t *)task->registers.esp;
	task_page_task(task);
	result = (void *)sp_ptr[index];
	kernel_page();
	return result;
}

void *task_virtual_addr_to_physical(struct thread_info *task, void *virtual_addr)
{
	return paging_get_physical_address(task->page_directory, virtual_addr);
}

/*
 * 这是一个非常简单的调度器；调度器只需要处理两件事，1.当前进程执行多场时间，2.下一个进程如何选择
 * 该调度器会在当前进程发生异常或结束时触发调度，下一个进程直接选择链表中的下一个进程；
 * 由于在process_terminal函数中删除了task，所以现在的current_task就是下一个task
 * 还要执行process_switch(next_task->process)，因为task和process是绑定的
 */
void task_next()
{
	struct thread_info* next_task = current_task;
	if (!next_task)
		panic("No more tasks!\n");
	
	process_switch(next_task->process);
	task_switch(next_task);
	task_return(&next_task->registers);
}