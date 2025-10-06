#include "exec.h"
#include "vfs.h"
#include "kmalloc.h"
#include "paging.h"
#include "task.h"
#include "pmm.h"

// Helper: copy string
static void strcpy_local(char *dest, const char *src)
{
    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Helper: get string length
static int strlen_local(const char *s)
{
    int len = 0;
    while (s[len])
    {
        len++;
    }
    return len;
}

// Load and execute a program
int exec_load(const char *path, const char **argv)
{
    if (!path)
    {
        return -1;
    }

    // Open the executable file
    struct file *f = vfs_open(path, 0);
    if (!f)
    {
        return -1; // File not found
    }

    // Read executable header
    struct exec_header header;
    if (vfs_read(f, &header, sizeof(header)) != sizeof(header))
    {
        vfs_close(f);
        return -1; // Failed to read header
    }

    // Verify magic number
    if (header.magic != EXEC_MAGIC)
    {
        vfs_close(f);
        return -1; // Invalid executable format
    }

    // Get current task
    struct task *current = task_get_current();
    if (!current)
    {
        vfs_close(f);
        return -1;
    }

    // Allocate new page directory for the process
    page_directory *old_dir = (page_directory *)current->regs.cr3;
    page_directory *kernel_dir = paging_get_kernel_directory();
    page_directory *new_dir;

    // If currently using kernel directory, create new one
    // Otherwise reuse the existing one (clean it up)
    if (old_dir == kernel_dir || old_dir == 0)
    {
        new_dir = (page_directory *)pmm_alloc_block();
        if (!new_dir)
        {
            vfs_close(f);
            return -1;
        }

        // Clear new directory
        for (int i = 0; i < 1024; i++)
        {
            new_dir->entries[i] = 0;
        }

        // Share kernel space (entries 768-1023)
        for (int i = 768; i < 1024; i++)
        {
            new_dir->entries[i] = kernel_dir->entries[i];
        }
    }
    else
    {
        // Reuse existing directory, but clear user space
        new_dir = old_dir;
        for (int i = 0; i < 768; i++)
        {
            if (new_dir->entries[i] & 0x1) // Present
            {
                // Free the page table (simplified - should free pages too)
                void *pt = (void *)(new_dir->entries[i] & 0xFFFFF000);
                pmm_free_block(pt);
                new_dir->entries[i] = 0;
            }
        }
    }

    // Switch to new page directory
    current->regs.cr3 = (uint32_t)new_dir;
    paging_switch_directory(new_dir);

    // Allocate and map text section
    uint32_t text_pages = (header.text_size + 0xFFF) / 0x1000;
    for (uint32_t i = 0; i < text_pages; i++)
    {
        void *phys = pmm_alloc_block();
        if (!phys)
        {
            vfs_close(f);
            return -1;
        }
        paging_map_page(phys, (void *)(USER_TEXT_START + i * 0x1000),
                        PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    // Read text section
    if (header.text_size > 0)
    {
        vfs_read(f, (void *)USER_TEXT_START, header.text_size);
    }

    // Allocate and map data section
    if (header.data_size > 0)
    {
        uint32_t data_pages = (header.data_size + 0xFFF) / 0x1000;
        for (uint32_t i = 0; i < data_pages; i++)
        {
            void *phys = pmm_alloc_block();
            if (!phys)
            {
                vfs_close(f);
                return -1;
            }
            paging_map_page(phys, (void *)(USER_DATA_START + i * 0x1000),
                            PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        }

        // Read data section
        vfs_read(f, (void *)USER_DATA_START, header.data_size);
    }

    // Allocate and map BSS section (zero-initialized)
    if (header.bss_size > 0)
    {
        uint32_t bss_start = USER_DATA_START + header.data_size;
        uint32_t bss_pages = (header.bss_size + 0xFFF) / 0x1000;
        for (uint32_t i = 0; i < bss_pages; i++)
        {
            void *phys = pmm_alloc_block();
            if (!phys)
            {
                vfs_close(f);
                return -1;
            }
            paging_map_page(phys, (void *)(bss_start + i * 0x1000),
                            PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

            // Zero out BSS
            uint32_t *bss = (uint32_t *)(bss_start + i * 0x1000);
            for (int j = 0; j < 1024; j++)
            {
                bss[j] = 0;
            }
        }
    }

    // Allocate and map stack
    uint32_t stack_size = header.stack_size > 0 ? header.stack_size : 0x4000; // Default 16KB
    uint32_t stack_pages = (stack_size + 0xFFF) / 0x1000;
    uint32_t stack_bottom = USER_STACK_TOP - stack_size;

    for (uint32_t i = 0; i < stack_pages; i++)
    {
        void *phys = pmm_alloc_block();
        if (!phys)
        {
            vfs_close(f);
            return -1;
        }
        paging_map_page(phys, (void *)(stack_bottom + i * 0x1000),
                        PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    vfs_close(f);

    // Set up initial registers for the new program
    current->regs.eip = header.entry;
    current->regs.esp = USER_STACK_TOP;
    current->regs.ebp = USER_STACK_TOP;
    current->regs.eax = 0;
    current->regs.ebx = 0;
    current->regs.ecx = 0;
    current->regs.edx = 0;
    current->regs.esi = 0;
    current->regs.edi = 0;
    current->regs.eflags = 0x202; // Interrupts enabled

    // Update process name
    if (argv && argv[0])
    {
        int i;
        for (i = 0; i < 31 && argv[0][i]; i++)
        {
            current->name[i] = argv[0][i];
        }
        current->name[i] = '\0';
    }

    return 0;
}

// System call: execve
int sys_execve(const char *path, const char **argv, const char **envp)
{
    (void)envp; // Unused for now

    int result = exec_load(path, argv);
    if (result < 0)
    {
        return -1;
    }

    // If successful, exec never returns to the caller
    // The process's address space has been replaced
    // We need to jump to the new program's entry point

    // This is tricky: we need to return from the syscall
    // but to the NEW program's entry point, not the caller

    // The registers are already set up in exec_load
    // Just return 0 to indicate success
    // The return will go to the new EIP

    return 0;
}
