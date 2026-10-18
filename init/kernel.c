#include "asm-x86/idt.h"
#include "asm-x86/io.h"
#include "asm-x86/gdt.h"
#include "asm-x86/tss.h"

#include "kernel/kernel.h"
#include "kernel/print.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "kernel/pparser.h"
#include "kernel/disk.h"
#include "kernel/streamer.h"
#include "kernel/file.h"
#include "kernel/string.h"
#include "kernel/config.h"
#include "kernel/sched.h"
#include "kernel/isr80h.h"
#include "kernel/keyboard.h"

struct paging_4gb_chunk *kernel_chunk = 0;

struct tss tss;
struct gdt gdt_real[KERNEL_TOTAL_GDT_SEGMENTS];
struct gdt_structured gdt_structured[KERNEL_TOTAL_GDT_SEGMENTS] = {
	{.base = 0x00, .limit = 0x00, .type = 0x00},			/* NULL */
	{.base = 0x00, .limit = 0xFFFFFFFF, .type = 0x9a},		/* kernel code */
	{.base = 0x00, .limit = 0xFFFFFFFF, .type = 0x92},		/* kernel data */
	{.base = 0x00, .limit = 0xFFFFFFFF, .type = 0xF8},		/* user code */
	{.base = 0x00, .limit = 0xFFFFFFFF, .type = 0xF2},		/* user data */
	{.base = (uint32_t)&tss, .limit = sizeof(tss), .type = 0xE9}, 	/* tss */
};

void kernel_page()
{
	kernel_register();
	paging_switch(kernel_chunk);
}

void kernel_test()
{
	terminal_print("Hello World!\n");

	/* test *********************************************************/
	char* ptr = kzalloc(4096); 
    	paging_set(paging_4gb_chunk_get_directory(kernel_chunk), (void*)0x1000, (uint32_t)ptr | PAGING_ACCESS_FROM_ALL | PAGING_IS_PRESENT | PAGING_IS_WRITEABLE);
	/***************************************************************/

	/* test *********************************************************/
	char *ptr2 = (char *)(0x1000);
	ptr2[0] = 'A';
	ptr2[1] = 'B';
	ptr2[2] = '\n';
	ptr2[3] = 0;
	terminal_print(ptr2);
	terminal_print(ptr);
	terminal_print(ptr);
	/***************************************************************/

	// struct path_root *root_path = pathparser_parse("0:/bin/shell.exe", NULL);
	// if (!root_path)
	// 	return;

	// terminal_print_num(root_path->drive_no);
	// struct path_part *path_part = root_path->first;
	// while (path_part) {
	// 	terminal_print_endl(path_part->part);
	// 	path_part = path_part->next;
	// }

	// struct disk_stream *stream = diskstream_new(0);
	// diskstream_seek(stream, 0x201);
	// unsigned char c = 0;
	// diskstream_read(stream, &c, 1);

	char buf[30];
	// strcpy(buf, "Hello World!");
	// terminal_print_endl(buf);

	int fd = fopen("0:/hello.txt", "r");
	terminal_print_num(fd);
	if (fd)
		terminal_print_endl("We opened /hello.txt");
	int res = 0;
	res = fclose(fd);
	if (res < 0)
		terminal_print_endl("fclose fail");

	fd = fopen("0:/home/hello_2.txt", "r");
	terminal_print_num(fd);
	if (fd)
		terminal_print_endl("We opened /home/hello_2.txt");

	res = fseek(fd, 2, SEEK_SET);
	if (res < 0)
		terminal_print_endl("seek set fail");
	res = fseek(fd, 1, SEEK_CUR);
	if (res < 0)
		terminal_print_endl("seek cur fail");
	res = fread(buf, 20, 1, fd);
	if (res < 0)
		terminal_print_endl("fread fail");
	buf[11] = 0x00;
	terminal_print_endl(buf);
	struct file_stat stat;
	res = fstat(fd, &stat);
	if (res < 0)
		terminal_print_endl("fstat fail");
	terminal_print_num(stat.filesize);
	terminal_print_num(stat.flags);
	res = fclose(fd);
	if (res < 0)
		terminal_print_endl("fclose fail");
}

void kernel_main()
{
	terminal_initialize();

	memset(gdt_real, 0x00, sizeof(gdt_real));
	gdt_structured_to_gdt(gdt_real, gdt_structured, KERNEL_TOTAL_GDT_SEGMENTS);
	gdt_load(gdt_real, sizeof(gdt_real) - 1);

	kheap_init();

	fs_init();

	disk_search_and_init();

	idt_init();

	/* 初始化tss内核栈信息*/
	memset(&tss, 0x00, sizeof(tss));
	tss.esp0 = 0x600000;
	tss.ss0 = KERNEL_DATA_SELECTOR;

	/* 加载tss*/
	tss_load(0x28);

	kernel_chunk = paging_new_4gb_chunk(PAGING_IS_WRITEABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);

	paging_switch(kernel_chunk);

	enable_paging();

	keyboard_init();

	isr80h_register_commands();

	struct task_struct *process = NULL;
	int res = process_load_switch("0:/shell.elf", &process);
	if (res < 0)
		panic("Failed to load shell.elf\n");
	
	// kernel_test();

	task_run_first_ever_task();

	terminal_print_endl("end");
}