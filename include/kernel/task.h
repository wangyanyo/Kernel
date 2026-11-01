#ifndef __KERNEL_TASK_H
#define __KERNEL_TASK_H

#include "kernel/config.h"
#include "kernel/paging.h"
#include "kernel/list.h"

struct interrupt_frame;

struct registers {
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;

	uint32_t ip;
	uint32_t cs;
	uint32_t flags;
	uint32_t esp;
	uint32_t ss;
};

struct task_struct;

struct thread_info {
	struct paging_4gb_chunk *page_directory;
	struct registers registers;
	struct task_struct *process;
	struct list_head list;
};

struct thread_info *task_new(struct task_struct *process);
struct thread_info *task_current();
struct thread_info *task_get_next();
int task_free(struct thread_info *task);

void task_return(struct registers* regs);
void restore_general_purpose_registers(struct registers* regs);
void user_registers();

int task_switch(struct thread_info *task);
int task_page();
int task_page_task(struct thread_info *task);
int task_run_first_ever_task();

void task_current_save_state(struct interrupt_frame *frame);

int copy_string_from_task(struct thread_info *task, void *virtual, void *phys, int max);

void *task_get_stack_item(struct thread_info *task, int index);

void *task_virtual_addr_to_physical(struct thread_info *task, void *virtual_addr);

void task_next();

#endif