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

// Switch to a different page directory
void paging_switch_directory(page_directory *dir)
{
    current_directory = dir;
    __asm__ volatile("mov %0, %%cr3" : : "r"(dir) : "memory");
}

// Clone a page directory (for fork)
page_directory *paging_clone_directory(page_directory *src)
{
    if (!src)
    {
        return 0;
    }

    // Allocate new page directory
    page_directory *new_dir = (page_directory *)pmm_alloc_block();
    if (!new_dir)
    {
        return 0;
    }

    // Clear new directory
    for (int i = 0; i < PAGES_PER_DIR; i++)
    {
        new_dir->entries[i] = 0;
    }

    // Copy all page directory entries
    for (int i = 0; i < PAGES_PER_DIR; i++)
    {
        // Skip if page table not present
        if (!(src->entries[i] & PAGE_PRESENT))
        {
            continue;
        }

        // For kernel space (top 256 entries), share the same page tables
        // This ensures kernel is accessible from all processes
        if (i >= 768) // 768 * 4MB = 3GB (kernel space starts at 3GB)
        {
            new_dir->entries[i] = src->entries[i];
            continue;
        }

        // For user space, we need to copy the page table
        page_table *src_table = (page_table *)(src->entries[i] & 0xFFFFF000);

        // Allocate new page table
        page_table *new_table = (page_table *)pmm_alloc_block();
        if (!new_table)
        {
            // Clean up and return failure
            paging_free_directory(new_dir);
            return 0;
        }

        // Clear new table
        for (int j = 0; j < PAGES_PER_TABLE; j++)
        {
            new_table->entries[j] = 0;
        }

        // Copy all page table entries
        for (int j = 0; j < PAGES_PER_TABLE; j++)
        {
            if (!(src_table->entries[j] & PAGE_PRESENT))
            {
                continue;
            }

            // Allocate new physical page
            void *new_page = pmm_alloc_block();
            if (!new_page)
            {
                // Clean up
                pmm_free_block(new_table);
                paging_free_directory(new_dir);
                return 0;
            }

            // Copy page content
            void *src_page = (void *)(src_table->entries[j] & 0xFFFFF000);
            uint32_t *src_ptr = (uint32_t *)src_page;
            uint32_t *dst_ptr = (uint32_t *)new_page;

            for (int k = 0; k < (PAGE_SIZE / sizeof(uint32_t)); k++)
            {
                dst_ptr[k] = src_ptr[k];
            }

            // Set new page table entry with same flags
            uint32_t flags = src_table->entries[j] & 0xFFF;
            new_table->entries[j] = ((uint32_t)new_page) | flags;
        }

        // Set page directory entry
        uint32_t flags = src->entries[i] & 0xFFF;
        new_dir->entries[i] = ((uint32_t)new_table) | flags;
    }

    return new_dir;
}

// Free a page directory and all its page tables
void paging_free_directory(page_directory *dir)
{
    if (!dir || dir == &kernel_directory)
    {
        return; // Don't free kernel directory
    }

    // Free all user space page tables (entries 0-767)
    for (int i = 0; i < 768; i++)
    {
        if (!(dir->entries[i] & PAGE_PRESENT))
        {
            continue;
        }

        page_table *table = (page_table *)(dir->entries[i] & 0xFFFFF000);

        // Free all pages in this table
        for (int j = 0; j < PAGES_PER_TABLE; j++)
        {
            if (table->entries[j] & PAGE_PRESENT)
            {
                void *page = (void *)(table->entries[j] & 0xFFFFF000);
                pmm_free_block(page);
            }
        }

        // Free the page table itself
        pmm_free_block(table);
    }

    // Free the page directory
    pmm_free_block(dir);
}
