#ifndef __KERNEL_KEYBOARD_H
#define __KERNEL_KEYBOARD_H

struct task_struct;

#define KEYBOARD_CAPS_LOCK_ON 1
#define KEYBOARD_CAPS_LOCK_OFF 0

typedef int KEYBOARD_CAPS_LOCK_STATE;

typedef int (*KEYBOARD_INIT_FUNCTION)();

struct keyboard {
	KEYBOARD_INIT_FUNCTION init;
	char name[20];
	struct keyboard *next;
	KEYBOARD_CAPS_LOCK_STATE state;
};

void keyboard_init();
int keyboard_insert(struct keyboard *keyboard);
void keyboard_push(char c);
char keyboard_pop();
void keyboard_backspace(struct task_struct *tsk);
void keyboard_set_capslock(struct keyboard *keyboard, KEYBOARD_CAPS_LOCK_STATE state);
KEYBOARD_CAPS_LOCK_STATE keyboard_get_capslock(struct keyboard *keyboard);

#endif