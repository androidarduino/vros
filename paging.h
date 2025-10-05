#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

// Page size (4KB)
#define PAGE_SIZE 4096

// Page directory and page table entries
#define PAGES_PER_TABLE 1024
#define PAGES_PER_DIR 1024

// Page flags
#define PAGE_PRESENT 0x01
#define PAGE_WRITE 0x02
#define PAGE_USER 0x04
#define PAGE_ACCESSED 0x20
#define PAGE_DIRTY 0x40

// Page table entry
typedef uint32_t pt_entry;

// Page directory entry
typedef uint32_t pd_entry;

// Page table
typedef struct page_table
{
    pt_entry entries[PAGES_PER_TABLE];
} page_table;

// Page directory
typedef struct page_directory
{
    pd_entry entries[PAGES_PER_DIR];
} page_directory;

// Function declarations
void paging_init(void);
void paging_map_page(void *phys, void *virt, uint32_t flags);
void paging_unmap_page(void *virt);
void *paging_get_physical_address(void *virt);
void paging_enable(void);
page_directory *paging_get_kernel_directory(void);

#endif // PAGING_H
