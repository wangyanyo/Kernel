#ifndef __ASM_I386_IDT_H
#define __ASM_I386_IDT_H

#include <stdint.h>

struct idt_desc {
	uint16_t offset_1;
	uint16_t selector;
	uint8_t  zero;
	uint8_t  type_attributes;
	uint16_t offset_2;
}__attribute__((packed));

struct idtr_desc {
	uint16_t limit;
	uint32_t base;
}__attribute__((packed));

struct interrupt_frame
{
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t reserved;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t ip;
	uint32_t cs;
	uint32_t flags;
	uint32_t esp;
	uint32_t ss;
} __attribute__((packed));

struct interrupt_frame;
typedef void *(*ISR80H_COMMAND)(struct interrupt_frame *frame);
typedef void (*INTERRUPT_CALLBACK_FUNCTION)(struct interrupt_frame *frame);

void idt_init();
void idt_set(int interrupt_no, void* address);
void enable_interrupts();
void disable_interrupts();
void isr80h_register_command(int command, ISR80H_COMMAND command_func);
int idt_register_interrupt_callback(int interrupt, INTERRUPT_CALLBACK_FUNCTION interrupt_callback);

#endif