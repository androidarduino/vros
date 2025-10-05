#include "paging.h"
#include "pmm.h"

// Kernel page directory (identity mapped for first 4MB)
static page_directory kernel_directory __attribute__((aligned(4096)));
static page_table kernel_tables[4] __attribute__((aligned(4096)));

// Current page directory
static page_directory *current_directory = 0;

// Get page table entry
static pt_entry *paging_get_page(void *virt, int create)
{
    uint32_t addr = (uint32_t)virt;
    uint32_t dir_index = addr >> 22;             // Top 10 bits
    uint32_t table_index = (addr >> 12) & 0x3FF; // Middle 10 bits

    // Check if page table exists
    if (!(current_directory->entries[dir_index] & PAGE_PRESENT))
    {
        if (!create)
        {
            return 0;
        }

        // Allocate new page table
        void *table = pmm_alloc_block();
        if (!table)
        {
            return 0;
        }

        // Clear page table
        uint32_t *ptr = (uint32_t *)table;
        for (int i = 0; i < PAGES_PER_TABLE; i++)
        {
            ptr[i] = 0;
        }

        // Add to page directory
        current_directory->entries[dir_index] = (uint32_t)table | PAGE_PRESENT | PAGE_WRITE;
    }

    // Get page table address
    page_table *table = (page_table *)(current_directory->entries[dir_index] & 0xFFFFF000);

    return &table->entries[table_index];
}

// Map a physical page to a virtual address
void paging_map_page(void *phys, void *virt, uint32_t flags)
{
    pt_entry *page = paging_get_page(virt, 1);
    if (page)
    {
        *page = ((uint32_t)phys & 0xFFFFF000) | flags | PAGE_PRESENT;
    }
}

// Unmap a virtual page
void paging_unmap_page(void *virt)
{
    pt_entry *page = paging_get_page(virt, 0);
    if (page)
    {
        *page = 0;

        // Flush TLB
        __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
    }
}

// Get physical address from virtual address
void *paging_get_physical_address(void *virt)
{
    pt_entry *page = paging_get_page(virt, 0);
    if (!page || !(*page & PAGE_PRESENT))
    {
        return 0;
    }

    return (void *)((*page & 0xFFFFF000) | ((uint32_t)virt & 0xFFF));
}

// Initialize paging
void paging_init(void)
{
    // Clear kernel page directory
    for (int i = 0; i < PAGES_PER_DIR; i++)
    {
        kernel_directory.entries[i] = 0;
    }

    // Identity map first 16MB (0x00000000 - 0x01000000)
    // This covers the kernel and video memory
    for (int i = 0; i < 4; i++)
    {
        // Clear page table
        for (int j = 0; j < PAGES_PER_TABLE; j++)
        {
            kernel_tables[i].entries[j] = 0;
        }

        // Map 1024 pages (4MB)
        for (int j = 0; j < PAGES_PER_TABLE; j++)
        {
            uint32_t phys = (i * PAGES_PER_TABLE + j) * PAGE_SIZE;
            kernel_tables[i].entries[j] = phys | PAGE_PRESENT | PAGE_WRITE;
        }

        // Add page table to directory
        kernel_directory.entries[i] = (uint32_t)&kernel_tables[i] | PAGE_PRESENT | PAGE_WRITE;
    }

    current_directory = &kernel_directory;
}

// Enable paging
void paging_enable(void)
{
    // Load page directory into CR3
    __asm__ volatile("mov %0, %%cr3" : : "r"(&kernel_directory));

    // Enable paging by setting bit 31 of CR0
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

// Get kernel page directory
page_directory *paging_get_kernel_directory(void)
{
    return &kernel_directory;
}
