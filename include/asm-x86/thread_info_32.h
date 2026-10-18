#ifndef __ASM_I386_THREAD_INFO_H
#define __ASM_I386_THREAD_INFO_H

#include "kernel/compiler.h"

struct thread_info {
	struct task_struct	*task;		/* main task structure */
};

#define THREAD_SIZE		(8192)

register unsigned long current_stack_pointer asm("esp") __attribute_used__;

static inline struct thread_info *current_thread_info(void)
{
	return (struct thread_info *)(current_stack_pointer & ~(THREAD_SIZE - 1));
}

#endif