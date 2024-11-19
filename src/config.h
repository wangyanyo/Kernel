#ifndef CONFIG_H
#define CONFIG_H

#define KERNEL_TOTAL_INTERRUPTS 512

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10

#define KERNEL_HEAP_SIZE_BYTES 104857600
#define KERNEL_HEAP_BLOCK_SIZE 4096
#define KERNEL_HEAP_ADDRESS 0x1000000
// 内存表放在引导程序后面
#define KERNEL_HEAP_TABLE_ADDRESS 0x00007E00

#define KERNEL_SECTOR_SIZE 512

#define KERNEL_MAX_PATH 108

#define KERNEL_MAX_FILESYSTEM 12
#define KERNEL_MAX_FILE_DESCRIPTORS 512

#define KERNEL_TOTAL_GDT_SEGMENTS 6

#endif
