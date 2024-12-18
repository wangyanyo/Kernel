#include "heap.h"
#include "kernel.h"
#include "memory/memory.h"

struct heap kernel_heap;
struct heap_table kernel_heap_table;

void kheap_init() {
    int total_table_entries = KERNEL_HEAP_SIZE_BYTES / KERNEL_HEAP_BLOCK_SIZE;
    kernel_heap_table.entries = (HEAP_BLOCK_TABLE_ENTRY*)KERNEL_HEAP_TABLE_ADDRESS;
    kernel_heap_table.total = total_table_entries;

    void* end = (void*)(KERNEL_HEAP_ADDRESS + KERNEL_HEAP_SIZE_BYTES);

    int res = heap_create(&kernel_heap, (void*)KERNEL_HEAP_ADDRESS, end, &kernel_heap_table);

    if(res < 0) {
        print("Failed to create heap\n");
    }
}

void* kmalloc(size_t size) {
    return heap_malloc(&kernel_heap, size);
}

void kfree(void* ptr) {
    if(ptr == 0) return;
    heap_free(&kernel_heap, ptr);
}

void* kzalloc(size_t size) {
    #warning 一次分配的空间太大了, 内核有很多小空间分配需求, 一次分配一页会造成很大的空间浪费, 这是以后要更新的点
    void* ptr = kmalloc(size);
    if(!ptr) {
        return 0;
    }
    memset(ptr, 0x00, size);
    return ptr;
}