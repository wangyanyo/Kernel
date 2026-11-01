#include "asm-x86/idt.h"

#include "kernel/isr80h.h"
#include "kernel/config.h"
#include "kernel/kernel.h"
#include "kernel/string.h"
#include "kernel/sched.h"
#include "kernel/sched.h"
#include "kernel/keyboard.h"
#include "kernel/status.h"

extern void task_return(struct registers* regs);

void *isr80h_command0_sum(struct interrupt_frame *frame)
{
	uint32_t item_1 = (uint32_t)task_get_stack_item(task_current(), 0);
	uint32_t item_2 = (uint32_t)task_get_stack_item(task_current(), 1);
	return (void *)(item_1 + item_2);
}

void *isr80h_command1_print(struct interrupt_frame *frame)
{
	char *user_string_virt_ptr = task_get_stack_item(task_current(), 0);
	char buf[1024];
	copy_string_from_task(task_current(), user_string_virt_ptr, buf, sizeof(buf));
	terminal_print(buf);
	return 0;
}

void *isr80h_command2_getkey(struct interrupt_frame *frame)
{
	char c = keyboard_pop();
	return (void *)((int)c);
}

void *isr80h_command3_putchar(struct interrupt_frame *frame)
{
	void *item = task_get_stack_item(task_current(), 0);
	terminal_print_char((char)((int)item));
	return 0;
}

void *isr80h_command4_malloc(struct interrupt_frame *frame)
{
	void *item = task_get_stack_item(task_current(), 0);
	return task_malloc(task_current(), (size_t)item);
}

void *isr80h_command5_free(struct interrupt_frame *frame)
{
	void *item = task_get_stack_item(task_current(), 0);
	task_free(task_current(), item);
	return NULL;
}

void *isr80h_command6_process_load_start(struct interrupt_frame *frame)
{
	int ret = 0;
	char *user_data_ptr = NULL;
	char filepath[KERNEL_MAX_PATH] = "0:/";
	struct task_struct *tsk = NULL;

	user_data_ptr = task_get_stack_item(task_current(), 0);
	ret = copy_string_from_task(task_current(), user_data_ptr, filepath + 3, KERNEL_MAX_PATH - 3);
	if (ret < 0)
		goto out;

	ret = task_load(filepath, &tsk);
	if (ret < 0)
		goto out;
	
	task_switch(tsk);
	task_return(&tsk->registers);

out:
	return (void *)ret;
}

void *isr80h_command7_invake_system_command(struct interrupt_frame *frame)
{
	// 首先加载进程，然后设置argument，然后返回
	int ret = 0;
	struct command_argument *root_command_argument;
	char filepath[KERNEL_MAX_PATH] = "0:/";
	struct task_struct *tsk = NULL;
	char *program_name;

	root_command_argument = task_virtual_addr_to_physical(task_current(),
		task_get_stack_item(task_current(), 0));
	if (!root_command_argument || strlen(root_command_argument->argument) == 0) {
		ret = -EINVAGS;
		goto out;
	}

	program_name = root_command_argument->argument;
	strncpy(filepath + 3, program_name, KERNEL_MAX_PATH - 3);
	ret = task_load(filepath, &tsk);
	if (ret < 0)
		goto out;

	task_switch(tsk);

	ret = task_inject_argument(tsk, root_command_argument);
	if (ret < 0)
		goto out;
	
	task_return(&tsk->registers);
out:
	return (void *)ret;
	
}

void *isr80h_command8_get_program_argument(struct interrupt_frame *frame)
{
	struct task_argument *argument = task_virtual_addr_to_physical(task_current(),
		task_get_stack_item(task_current(), 0));
	task_get_argument(task_current(), &argument->argc, &argument->argv);
	return 0;
}

/* 用户程序结束时，会向内核传递一个返回值，该返回值会通过内核转交给父进程 */
void *isr80h_command9_exit(struct interrupt_frame *frame)
{
	void *item = task_get_stack_item(task_current(), 0);
	struct task_struct *tsk = task_current();

	task_terminate(tsk);
	tsk->res = (int)item;

	task_next();

	return 0;
}

void isr80h_register_commands()
{
	isr80h_register_command(SYSTEM_COMMAND0_SUM, isr80h_command0_sum);
	isr80h_register_command(SYSTEM_COMMAND1_PRINT, isr80h_command1_print);
	isr80h_register_command(SYSTEM_COMMAND2_GETKEY, isr80h_command2_getkey);
	isr80h_register_command(SYSTEM_COMMAND3_PUTCHAR, isr80h_command3_putchar);
	isr80h_register_command(SYSTEM_COMMAND4_MALLOC, isr80h_command4_malloc);
	isr80h_register_command(SYSTEM_COMMAND5_FREE, isr80h_command5_free);
	isr80h_register_command(STSTEM_COMMAND6_PROCESS_LOAD_START, isr80h_command6_process_load_start);
	isr80h_register_command(SYSTEM_COMMAND7_INVAKE_SYSTEM_COMMAND, isr80h_command7_invake_system_command);
	isr80h_register_command(SYSTEM_COMMAND8_GET_PROGRAM_ARGUMENT, isr80h_command8_get_program_argument);
	isr80h_register_command(SYSTEM_COMMAND9_EXIT, isr80h_command9_exit);
}