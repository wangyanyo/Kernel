#include "asm-x86/idt.h"
#include "asm-x86/io.h"

#include "kernel/config.h"
#include "kernel/print.h"
#include "kernel/string.h"
#include "kernel/sched.h"
#include "kernel/kernel.h"
#include "kernel/status.h"
#include "kernel/sched.h"

struct idt_desc idt_descriptors[KERNEL_TOTAL_INTERRUPTS];
struct idtr_desc idtr_descriptor;

static ISR80H_COMMAND isr80h_commands[KERNEL_MAX_ISR80H_COMMANDS];

static INTERRUPT_CALLBACK_FUNCTION interrupt_callbacks[KERNEL_TOTAL_INTERRUPTS];

extern void *interrupt_pointer_table[KERNEL_TOTAL_INTERRUPTS];

extern void idt_load(struct idtr_desc* ptr);
extern void int21h();
extern void no_interrupt();
extern void isr80h_wrapper();

static void idt_handle_exception()
{
	task_terminate(task_current());
	task_next();
}

void interrupt_handler(int interrupt, struct interrupt_frame *frame)
{
	if (interrupt_callbacks[interrupt] != NULL) {
		kernel_page();
		task_current_save_state(frame);
		interrupt_callbacks[interrupt](frame);
		task_page();
	}
        outb(0x20, 0x20);
}

void idt_set(int interrupt_no, void* address)
{
        struct idt_desc* desc = &idt_descriptors[interrupt_no];
        desc->offset_1 = (uint32_t)address & 0x0000ffff;
        desc->selector = KERNEL_CODE_SELECTOR;
        desc->zero = 0x00;
        desc->type_attributes = 0xEE;
        desc->offset_2 = (uint32_t)address >> 16;
}

void idt_init()
{
        memset(idt_descriptors, 0, sizeof(idt_descriptors));
        idtr_descriptor.limit = sizeof(idt_descriptors) - 1;
        idtr_descriptor.base = (uint32_t)idt_descriptors;

        for (int i = 0; i < KERNEL_TOTAL_INTERRUPTS; ++i)
                idt_set(i, interrupt_pointer_table[i]);

	idt_set(0x80, isr80h_wrapper);

        idt_load(&idtr_descriptor);

	for (int i = 0; i < 0x20; ++i)
		idt_register_interrupt_callback(i, idt_handle_exception);
}

int idt_register_interrupt_callback(int interrupt, INTERRUPT_CALLBACK_FUNCTION interrupt_callback)
{
	if (interrupt < 0 || interrupt >= KERNEL_TOTAL_INTERRUPTS)
		return -EINVAGS;

	interrupt_callbacks[interrupt] = interrupt_callback;
	return 0;
}

void isr80h_register_command(int command, ISR80H_COMMAND command_func)
{
	if (command < 0 || command >= KERNEL_MAX_ISR80H_COMMANDS)
		panic("The command is out of bounds\n");

	if (isr80h_commands[command])
		panic("Your attempting to overwrite an existing command\n");

	isr80h_commands[command] = command_func;
}

static void *isr80h_handle_command(int command, struct interrupt_frame *frame)
{
	if (command < 0 || command >= KERNEL_MAX_ISR80H_COMMANDS)
		return NULL;

	ISR80H_COMMAND command_func = isr80h_commands[command];
	if (!command_func)
		return NULL;
	
	return command_func(frame);
}

void *isr80h_handler(int command, struct interrupt_frame *frame)
{
	void *res = 0;
	/* 切换到内核态 */
	kernel_page();
	/* 更新当前进程的寄存器状态 */
	task_current_save_state(frame);
	/* 根据command路由到处理程序 */
	res = isr80h_handle_command(command, frame);
	/* 切换回用户态 */
	task_page();
	return res;
}