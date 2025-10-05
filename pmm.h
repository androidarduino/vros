#ifndef PMM_H
#define PMM_H

#include <stdint.h>

// Page size (4KB)
#define PAGE_SIZE 4096

// Convert address to page index
#define ADDR_TO_PAGE(addr) ((addr) / PAGE_SIZE)
#define PAGE_TO_ADDR(page) ((page) * PAGE_SIZE)

// Function declarations
void pmm_init(uint32_t mem_size);
void pmm_init_region(uint32_t base, uint32_t size);
void pmm_deinit_region(uint32_t base, uint32_t size);
void *pmm_alloc_block(void);
void pmm_free_block(void *addr);
uint32_t pmm_get_memory_size(void);
uint32_t pmm_get_used_blocks(void);
uint32_t pmm_get_free_blocks(void);
uint32_t pmm_get_total_blocks(void);

#endif // PMM_H
