#include "kmalloc.h"

// Heap start and size
static struct heap_block *heap_start = 0;
static uint32_t heap_size = 0;

// Minimum block size (to avoid too much fragmentation)
#define MIN_BLOCK_SIZE 16

// Initialize the heap
void kmalloc_init(void *start, uint32_t size)
{
    heap_start = (struct heap_block *)start;
    heap_size = size;

    // Initialize the first free block
    heap_start->size = size - sizeof(struct heap_block);
    heap_start->is_free = 1;
    heap_start->next = 0;
}

// Find a free block using first-fit algorithm
static struct heap_block *find_free_block(size_t size)
{
    struct heap_block *current = heap_start;

    while (current)
    {
        if (current->is_free && current->size >= size)
        {
            return current;
        }
        current = current->next;
    }

    return 0;
}

// Split a block if it's too large
static void split_block(struct heap_block *block, size_t size)
{
    // Only split if the remaining space is large enough
    if (block->size >= size + sizeof(struct heap_block) + MIN_BLOCK_SIZE)
    {
        struct heap_block *new_block = (struct heap_block *)((char *)block + sizeof(struct heap_block) + size);
        new_block->size = block->size - size - sizeof(struct heap_block);
        new_block->is_free = 1;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }
}

// Merge adjacent free blocks
static void merge_free_blocks(void)
{
    struct heap_block *current = heap_start;

    while (current && current->next)
    {
        if (current->is_free && current->next->is_free)
        {
            // Merge current with next
            current->size += sizeof(struct heap_block) + current->next->size;
            current->next = current->next->next;
        }
        else
        {
            current = current->next;
        }
    }
}

// Allocate memory
void *kmalloc(size_t size)
{
    if (size == 0)
    {
        return 0;
    }

    // Align size to 4 bytes
    if (size % 4 != 0)
    {
        size += 4 - (size % 4);
    }

    // Find a free block
    struct heap_block *block = find_free_block(size);
    if (!block)
    {
        return 0; // Out of memory
    }

    // Split the block if necessary
    split_block(block, size);

    // Mark as allocated
    block->is_free = 0;

    // Return pointer to data (after header)
    return (void *)((char *)block + sizeof(struct heap_block));
}

// Free memory
void kfree(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    // Get block header
    struct heap_block *block = (struct heap_block *)((char *)ptr - sizeof(struct heap_block));

    // Mark as free
    block->is_free = 1;

    // Merge adjacent free blocks
    merge_free_blocks();
}

// Get heap statistics
void kmalloc_stats(uint32_t *total, uint32_t *used, uint32_t *free_mem)
{
    *total = heap_size;
    *used = 0;
    *free_mem = 0;

    struct heap_block *current = heap_start;
    while (current)
    {
        if (current->is_free)
        {
            *free_mem += current->size + sizeof(struct heap_block);
        }
        else
        {
            *used += current->size + sizeof(struct heap_block);
        }
        current = current->next;
    }
}
