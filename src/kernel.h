#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 20

void kernel_main();
void print(const char* str);
void print_num(int num);
void panic(const char* msg);
void kernel_page();
void print_num_ln(int num);
void kernel_registers();
void terminal_writechar(char c, char color);

#define ERROR(value) (void*)value
#define ERROR_I(value) (int)value
#define ISERR(value) ((int)value < 0)

#define MIN(x, y) ((x) < (y)) ? (x) : (y)

#endif
