#ifndef __KERNEL_LIST_H
#define __KERNEL_LIST_H

struct list_head {
	struct list_head *next, *prev;
};

#endif