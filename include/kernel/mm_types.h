#ifndef __KERNEL_MM_TYPES_H
#define __KERNEL_MM_TYPES_H

#include <stddef.h>
#include "kernel/config.h"
#include "kernel/elfloader.h"
#include "kernel/list.h"

struct vm_area_struct {
	void *ptr;
	size_t size;
	struct list_head node;
};

struct mm_struct {
	void *pgd;
	struct vm_area_struct *vmas;
	char exe_path[KERNEL_MAX_PATH];
	struct elf_file *elf_file;
};

#endif