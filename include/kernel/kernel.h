#ifndef __KERNEL_KERNEL_H
#define __KERNEL_KERNEL_H

#include <stddef.h>
#include <stdint.h>
#include "kernel/print.h"

#define pr_err(msg) \
	terminal_print(__FILE__); \
	terminal_print(" "); \
	terminal_print(__func__); \
	terminal_print(":"); \
	terminal_print_num_without_endl(__LINE__); \
	terminal_print(" [E] "); \
	terminal_print(msg); \

#define pr_warn(msg) \
	terminal_print(__FILE__); \
	terminal_print(" "); \
	terminal_print(__func__); \
	terminal_print(":"); \
	terminal_print_num_without_endl(__LINE__); \
	terminal_print(" [W] "); \
	terminal_print(msg); \

#define pr_info(msg) \
	terminal_print(__FILE__); \
	terminal_print(" "); \
	terminal_print(__func__); \
	terminal_print(":"); \
	terminal_print_num_without_endl(__LINE__); \
	terminal_print(" [I] "); \
	terminal_print(msg); \

#define pr_debug(msg) \
	terminal_print(__FILE__); \
	terminal_print(" "); \
	terminal_print(__func__); \
	terminal_print(":"); \
	terminal_print_num_without_endl(__LINE__); \
	terminal_print(" [D] "); \
	terminal_print(msg); \

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

void kernel_main();

void kernel_page();
void kernel_register();

#endif