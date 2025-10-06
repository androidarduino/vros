#ifndef KMALLOC_H
#define KMALLOC_H

#include <stdint.h>
#include <stddef.h>

// Heap memory block header
struct heap_block
{
    uint32_t size;           // Size of the block (excluding header)
    uint32_t is_free;        // 1 if free, 0 if allocated
    struct heap_block *next; // Next block in the list
};

// Function declarations
void kmalloc_init(void *start, uint32_t size);
void *kmalloc(size_t size);
void kfree(void *ptr);
void kmalloc_stats(uint32_t *total, uint32_t *used, uint32_t *free);

#endif // KMALLOC_H
