#ifndef __KERNEL_SCHED_H
#define __KERNEL_SCHED_H

#include "sched.h"
#include "config.h"
#include "paging.h"
#include "list.h"

typedef enum {
	TASK_FILETYPE_ELF,
	TASK_FILETYPE_BINARY,
} TASK_FILETYPE;

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

struct vm_area_struct {
	void *ptr;
	size_t size;
};

struct command_argument {
	char argument[512];
	struct command_argument *next;
};

struct task_argument {
	int argc;
	char **argv;
};

struct task_struct {

	uint16_t id;

	char filename[KERNEL_MAX_PATH];

	struct vm_area_struct vm_area[KERNEL_MAX_PROGRAM_ALLOCATIONS];

	TASK_FILETYPE filetype;
	union {
		void *ptr;
		struct elf_file *elf_file;
	};
	uint32_t size;

	void *stack;

	struct paging_4gb_chunk *page_directory;

	struct registers registers;

	struct list_head list;

	struct keyboard_buffer {
		char buffer[KERNEL_KEYBOARD_BUFFER_SIZE];
		int head;
		int tail;
		int size;
	} keyboard;

	struct task_argument argument;

	int res;
};

struct task_struct *task_get(int pid);
struct task_struct *task_current();

int task_switch(struct task_struct *task);
void task_next();
int task_page();
int task_run_first_ever_task();
int task_load(char *filename, struct task_struct **tsk);
int task_terminate(struct task_struct *tsk);

void *task_malloc(struct task_struct *tsk, size_t size);
void task_free(struct task_struct *tsk, void *ptr);
void task_get_argument(struct task_struct *tsk, int *argc, char ***argv);
int task_inject_argument(struct task_struct *tsk, struct command_argument *root_argument);

void task_current_save_state(struct interrupt_frame *frame);
int copy_string_from_task(struct task_struct *tsk, void *virtual, void *phys, int max);
void *task_get_stack_item(struct task_struct *tsk, int index);
void *task_virtual_addr_to_physical(struct task_struct *task, void *virtual_addr);

#endif