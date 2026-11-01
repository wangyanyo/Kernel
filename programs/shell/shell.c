#include "shell.h"
#include "stdlib.h"
#include "stdio.h"
#include "kernel.h"
#include "string.h"

int main(int argc, char **argv)
{

	while (1) {
		print(">");
		char buf[1024];
		kernel_terminal_readline(buf, 1024, 1);
		print("\n");

		if (!strlen(buf))
			continue;

		kernel_system_run(buf);

		/*
		 * 这里可以通过wait系统调用获取子进程的返回值，并顺便清理struct task_struct结构
		 * 我理解僵尸进程是如何产生、如何清理的了，以及这里为什么需要pid，fork系统调用的必要性
		 * 多进程及其处理方式真是非常巧妙
		 */
		// ret = kernel_wait(pid);
		// if (ret < 0) {
		// 	printf("exec %s fail!, ret:%i\n", buf, ret);
		// }
	}
	return 0;
}