#include "pmm.h"

// Memory bitmap (each bit represents one 4KB page)
#define MAX_BLOCKS 32768 // 32768 blocks = 128MB
static uint32_t memory_map[MAX_BLOCKS / 32];

// Memory statistics
static uint32_t memory_size = 0;
static uint32_t used_blocks = 0;
static uint32_t max_blocks = 0;

// Set a bit in the bitmap
static inline void bitmap_set(uint32_t bit)
{
    memory_map[bit / 32] |= (1 << (bit % 32));
}

// Clear a bit in the bitmap
static inline void bitmap_clear(uint32_t bit)
{
    memory_map[bit / 32] &= ~(1 << (bit % 32));
}

// Test if a bit is set
static inline int bitmap_test(uint32_t bit)
{
    return memory_map[bit / 32] & (1 << (bit % 32));
}

// Find first free block
static int bitmap_first_free(void)
{
    for (uint32_t i = 0; i < max_blocks / 32; i++)
    {
        if (memory_map[i] != 0xFFFFFFFF)
        {
            for (int j = 0; j < 32; j++)
            {
                if (!(memory_map[i] & (1 << j)))
                {
                    return i * 32 + j;
                }
            }
        }
    }
    return -1;
}

// Initialize PMM
void pmm_init(uint32_t mem_size)
{
    memory_size = mem_size;
    max_blocks = mem_size / PAGE_SIZE;
    used_blocks = max_blocks;

    // Mark all blocks as used initially
    for (uint32_t i = 0; i < MAX_BLOCKS / 32; i++)
    {
        memory_map[i] = 0xFFFFFFFF;
    }
}

// Mark a region of memory as available
void pmm_init_region(uint32_t base, uint32_t size)
{
    uint32_t blocks = size / PAGE_SIZE;
    uint32_t start_block = base / PAGE_SIZE;

    for (uint32_t i = 0; i < blocks; i++)
    {
        bitmap_clear(start_block + i);
        used_blocks--;
    }
}

// Mark a region of memory as unavailable
void pmm_deinit_region(uint32_t base, uint32_t size)
{
    uint32_t blocks = size / PAGE_SIZE;
    uint32_t start_block = base / PAGE_SIZE;

    for (uint32_t i = 0; i < blocks; i++)
    {
        bitmap_set(start_block + i);
        used_blocks++;
    }
}

// Allocate a single 4KB block
void *pmm_alloc_block(void)
{
    if (used_blocks >= max_blocks)
    {
        return 0; // Out of memory
    }

    int block = bitmap_first_free();
    if (block == -1)
    {
        return 0; // No free blocks
    }

    bitmap_set(block);
    used_blocks++;

    return (void *)(block * PAGE_SIZE);
}

// Free a single 4KB block
void pmm_free_block(void *addr)
{
    uint32_t block = (uint32_t)addr / PAGE_SIZE;

    if (!bitmap_test(block))
    {
        return; // Already free
    }

    bitmap_clear(block);
    used_blocks--;
}

// Get total memory size
uint32_t pmm_get_memory_size(void)
{
    return memory_size;
}

// Get used blocks count
uint32_t pmm_get_used_blocks(void)
{
    return used_blocks;
}

// Get free blocks count
uint32_t pmm_get_free_blocks(void)
{
    return max_blocks - used_blocks;
}

// Get total blocks count
uint32_t pmm_get_total_blocks(void)
{
    return max_blocks;
}
