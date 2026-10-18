#include "kernel/keyboard.h"
#include "kernel/status.h"
#include "kernel/sched.h"
#include "kernel/task.h"
#include "keyboard/classic.h"

static struct keyboard *keyboard_list_head;
static struct keyboard *keyboard_list_tail;

void keyboard_init()
{
	keyboard_insert(classic_init());
}

int keyboard_insert(struct keyboard *keyboard)
{
	int res = 0;

	if (!keyboard || !keyboard->init) {
		res = -EINVAGS;
		goto out;
	}

	if (!keyboard_list_head) {
		keyboard_list_head = keyboard;
		keyboard_list_tail = keyboard;
	} else {
		keyboard_list_tail->next = keyboard;
		keyboard_list_tail = keyboard;
	}

	res = keyboard->init();

out:
	return res;
}

static int keyboard_get_next_index(int index)
{
	return (index + 1) % KERNEL_KEYBOARD_BUFFER_SIZE;
}

static int keyboard_get_last_index(int index)
{
	return (index + KERNEL_KEYBOARD_BUFFER_SIZE - 1) % KERNEL_KEYBOARD_BUFFER_SIZE;
}

void keyboard_push(char c)
{
	struct task_struct *process = current;
	if (!process || !c)
		return;
	
	if (process->keyboard.size == KERNEL_KEYBOARD_BUFFER_SIZE)
		return;

	int index = keyboard_get_next_index(process->keyboard.tail);
	process->keyboard.buffer[index] = c;
	process->keyboard.tail = index;
	process->keyboard.size++;
}

char keyboard_pop()
{
	struct task_struct *process = current;
	if (!process)
		return 0x00;

	if (process->keyboard.size == 0)
		return 0x00;

	int index = keyboard_get_next_index(process->keyboard.head);
	char c = process->keyboard.buffer[index];
	process->keyboard.buffer[index] = 0x00;
	process->keyboard.head = index;
	process->keyboard.size--;
	return c;
}
void keyboard_backspace(struct task_struct *process)
{
	if (!process)
		return;

	if (process->keyboard.size == 0)
		return;
	
	process->keyboard.buffer[process->keyboard.tail] = 0x00;
	int index = keyboard_get_last_index(process->keyboard.tail);
	process->keyboard.tail = index;
	process->keyboard.size--;
}

void keyboard_set_capslock(struct keyboard *keyboard, KEYBOARD_CAPS_LOCK_STATE state)
{
	keyboard->state = state;
}

KEYBOARD_CAPS_LOCK_STATE keyboard_get_capslock(struct keyboard *keyboard)
{
	return keyboard->state;
}