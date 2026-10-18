#include "asm-x86/idt.h"
#include "asm-x86/current.h"

#include "kernel/isr80h.h"
#include "kernel/config.h"
#include "kernel/kernel.h"
#include "kernel/string.h"
#include "kernel/task.h"
#include "kernel/sched.h"
#include "kernel/keyboard.h"
#include "kernel/status.h"

void *isr80h_command0_sum(struct interrupt_frame *frame)
{
	uint32_t item_1 = (uint32_t)task_get_stack_item(current, 0);
	uint32_t item_2 = (uint32_t)task_get_stack_item(current, 1);
	return (void *)(item_1 + item_2);
}

void *isr80h_command1_print(struct interrupt_frame *frame)
{
	char *user_string_virt_ptr = task_get_stack_item(current, 0);
	char buf[1024];
	copy_string_from_task(current, user_string_virt_ptr, buf, sizeof(buf));
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
	void *item = task_get_stack_item(current, 0);
	terminal_print_char((char)((int)item));
	return 0;
}

void *isr80h_command4_malloc(struct interrupt_frame *frame)
{
	void *item = task_get_stack_item(current, 0);
	return process_malloc(current->process, (size_t)item);
}

void *isr80h_command5_free(struct interrupt_frame *frame)
{
	void *item = task_get_stack_item(current, 0);
	process_free(current->process, item);
	return NULL;
}

void *isr80h_command6_process_load_start(struct interrupt_frame *frame)
{
	int ret = 0;
	char *user_data_ptr = NULL;
	char filepath[KERNEL_MAX_PATH] = "0:/";
	struct task_struct *process = NULL;

	user_data_ptr = task_get_stack_item(current, 0);
	ret = copy_string_from_task(current, user_data_ptr, filepath + 3, KERNEL_MAX_PATH - 3);
	if (ret < 0)
		goto out;

	ret = process_load_switch(filepath, &process);
	if (ret < 0)
		goto out;
	
	task_switch(process->task);
	task_return(&process->task->registers);

out:
	return (void *)ret;
}

void *isr80h_command7_invake_system_command(struct interrupt_frame *frame)
{
	// 首先加载进程，然后设置argument，然后返回
	int ret = 0;
	struct command_argument *root_command_argument;
	char filepath[KERNEL_MAX_PATH] = "0:/";
	struct task_struct *process = NULL;
	char *program_name;

	root_command_argument = task_virtual_addr_to_physical(current,
		task_get_stack_item(current, 0));
	if (!root_command_argument || strlen(root_command_argument->argument) == 0) {
		ret = -EINVAGS;
		goto out;
	}

	program_name = root_command_argument->argument;
	strncpy(filepath + 3, program_name, KERNEL_MAX_PATH - 3);
	ret = process_load_switch(filepath, &process);
	if (ret < 0)
		goto out;

	task_switch(process->task);

	ret = process_inject_argument(process, root_command_argument);
	if (ret < 0)
		goto out;
	
	task_return(&process->task->registers);
out:
	return (void *)ret;
	
}

void *isr80h_command8_get_program_argument(struct interrupt_frame *frame)
{
	struct process_argument *argument = task_virtual_addr_to_physical(current,
		task_get_stack_item(current, 0));
	// 这里有点thread_info的雏形，task->thread_info，process->task_struct，万变不器离其宗
	process_get_argument(current->process, &argument->argc, &argument->argv);
	return 0;
}

/* 用户程序结束时，会向内核传递一个返回值，该返回值会通过内核转交给父进程 */
void *isr80h_command9_exit(struct interrupt_frame *frame)
{
	void *item = task_get_stack_item(current, 0);
	struct task_struct *process = current->process;

	process_terminate(process);
	process->res = (int)item;

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