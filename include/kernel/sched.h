#ifndef __KERNEL_SCHED_H
#define __KERNEL_SCHED_H

#include "asm-x86/processor.h"
#include "asm-x86/current.h"
#include "asm-x86/atomic.h"
#include "kernel/config.h"
#include "kernel/mm_types.h"
#include "kernel/list.h"
#include "kernel/types.h"

struct command_argument {
	char argument[512];
	struct command_argument *next;
};

struct process_argument {
	int argc;
	char **argv;
};

struct task_struct {
        volatile long state;
        void *stack;
	atomic_t usage;
	unsigned int flags;
        
	struct mm_struct *mm;

        int exit_state;
	int exit_code;

        pid_t pid;

	struct list_head tasks;

        struct task_struct *parent;
        struct list_head children;
        struct list_head sibling;

	struct thread_struct thread;

	struct keyboard_buffer {
		char buffer[KERNEL_KEYBOARD_BUFFER_SIZE];
		int head;
		int tail;
		int size;
	} keyboard;

	struct process_argument argument;
};

struct task_struct *find_task_by_pid(pid_t nr);

int process_load_for_slot(char *filename, struct task_struct **process, int process_slot);
int process_load(char *filename, struct task_struct **process);
int process_load_switch(char *filename, struct task_struct **process);
void *paging_align_address(void *ptr);
void *process_malloc(struct task_struct *process, size_t size);
void process_free(struct task_struct *process, void *ptr);
void process_get_argument(struct task_struct *process, int *argc, char ***argv);
int process_inject_argument(struct task_struct *process, struct command_argument *root_argument);
int process_terminate(struct task_struct *process);
int process_switch(struct task_struct *process);

struct task_struct *task_new(struct task_struct *process);
struct task_struct *task_get_next();
int task_free(struct task_struct *task);

void task_return(struct registers* regs);
void restore_general_purpose_registers(struct registers* regs);
void user_registers();

int task_switch(struct task_struct *task);
int task_page();
int task_page_task(struct task_struct *task);
int task_run_first_ever_task();

void task_current_save_state(struct interrupt_frame *frame);

int copy_string_from_task(struct task_struct *task, void *virtual, void *phys, int max);

void *task_get_stack_item(struct task_struct *task, int index);

void *task_virtual_addr_to_physical(struct task_struct *task, void *virtual_addr);

void task_next();

#endif