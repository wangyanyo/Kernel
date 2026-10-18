#ifndef __KERNEL_CURRENT_32_H
#define __KERNEL_CURRENT_32_H

#include "asm-x86/thread_info.h"

static inline struct task_struct *get_current(void)
{
	return current_thread_info()->task;
}

#define current (get_current())

#endif