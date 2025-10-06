#include "shell.h"
#include "pmm.h"
#include "paging.h"
#include "kmalloc.h"
#include "task.h"
#include "syscall.h"
#include "vfs.h"
#include "usermode.h"
#include "ata.h"
#include "blkdev.h"
#include "vrfs.h"
#include "mount.h"
#include "ne2000.h"
#include "netif.h"
#include <stdint.h>

// External functions
extern void print_char(char c, int col, int row);
extern void print_string(const char *str, int row);

// Helper functions for port I/O
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Update hardware cursor position
static void update_cursor(int col, int row)
{
    uint16_t pos = row * 80 + col;

    // Send the high byte
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));

    // Send the low byte
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Command buffer
static char command_buffer[MAX_COMMAND_LENGTH];
static int command_pos = 0;

// Current cursor position
static int shell_row = 0;
static int shell_col = 0;

// Current working directory
static char current_dir[256] = "/";

// Helper function to compare strings
static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// Helper function to compare first n characters
static int strncmp(const char *s1, const char *s2, int n)
{
    while (n && *s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// Helper function to copy string
static void strcpy(char *dest, const char *src)
{
    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Helper function to get string length
static int strlen(const char *s)
{
    int len = 0;
    while (*s++)
    {
        len++;
    }
    return len;
}

// Normalize path (resolve relative paths, . and ..)
static void normalize_path(const char *path, char *result)
{
    if (!path || !result)
    {
        result[0] = '/';
        result[1] = '\0';
        return;
    }

    char temp[256];
    int is_absolute = (path[0] == '/');

    // Start with current dir if relative path
    if (!is_absolute)
    {
        strcpy(temp, current_dir);
        if (temp[strlen(temp) - 1] != '/')
        {
            int len = strlen(temp);
            if (len < 255)
            {
                temp[len] = '/';
                temp[len + 1] = '\0';
            }
        }

        // Append relative path
        int temp_len = strlen(temp);
        int i = 0;
        while (path[i] && temp_len < 255)
        {
            temp[temp_len++] = path[i++];
        }
        temp[temp_len] = '\0';
    }
    else
    {
        int i = 0;
        while (path[i] && i < 255)
        {
            temp[i] = path[i];
            i++;
        }
        temp[i] = '\0';
    }

    // Parse and resolve . and ..
    // Use static buffer instead of kmalloc to avoid memory issues
    char segment_storage[32][64];
    int segment_count = 0;

    char buffer[256];
    strcpy(buffer, temp);

    char *token = buffer;
    if (*token == '/')
        token++;

    while (*token)
    {
        char segment[64];
        int seg_len = 0;

        while (*token && *token != '/' && seg_len < 63)
        {
            segment[seg_len++] = *token++;
        }
        segment[seg_len] = '\0';

        if (*token == '/')
            token++;

        if (segment[0] == '\0' || (segment[0] == '.' && segment[1] == '\0'))
        {
            // Skip empty and "."
            continue;
        }
        else if (segment[0] == '.' && segment[1] == '.' && segment[2] == '\0')
        {
            // ".." - go up one level
            if (segment_count > 0)
                segment_count--;
        }
        else
        {
            // Regular segment
            if (segment_count < 32)
            {
                strcpy(segment_storage[segment_count], segment);
                segment_count++;
            }
        }
    }

    // Build result path
    if (segment_count == 0)
    {
        strcpy(result, "/");
    }
    else
    {
        result[0] = '\0';
        for (int i = 0; i < segment_count; i++)
        {
            int len = strlen(result);
            if (len < 254)
            {
                result[len] = '/';
                result[len + 1] = '\0';

                // Copy segment safely
                int j = 0;
                while (segment_storage[i][j] && len + 1 + j < 255)
                {
                    result[len + 1 + j] = segment_storage[i][j];
                    j++;
                }
                result[len + 1 + j] = '\0';
            }
        }
    }
}

// Scroll screen up by one line
static void shell_scroll(void)
{
    // Move all lines up by one
    for (int row = 1; row < 25; row++)
    {
        for (int col = 0; col < 80; col++)
        {
            // Read character from next line
            volatile char *vga = (volatile char *)0xB8000;
            int src_index = (row * 80 + col) * 2;
            int dst_index = ((row - 1) * 80 + col) * 2;

            // Copy character and attribute
            vga[dst_index] = vga[src_index];
            vga[dst_index + 1] = vga[src_index + 1];
        }
    }

    // Clear last line
    for (int col = 0; col < 80; col++)
    {
        print_char(' ', col, 24);
    }

    // Move cursor to last line
    shell_row = 24;
    shell_col = 0;
}

// Clear screen
void shell_clear_screen(void)
{
    for (int row = 0; row < 25; row++)
    {
        for (int col = 0; col < 80; col++)
        {
            print_char(' ', col, row);
        }
    }
    shell_row = 0;
    shell_col = 0;
    update_cursor(shell_col, shell_row);
}

// Print a string at current position
static void shell_print(const char *str)
{
    while (*str)
    {
        if (*str == '\n')
        {
            shell_col = 0;
            shell_row++;
            if (shell_row >= 25)
            {
                shell_scroll();
            }
        }
        else
        {
            print_char(*str, shell_col, shell_row);
            shell_col++;
            if (shell_col >= 80)
            {
                shell_col = 0;
                shell_row++;
                if (shell_row >= 25)
                {
                    shell_scroll();
                }
            }
        }
        str++;
    }
    // Update cursor position
    update_cursor(shell_col, shell_row);
}

// Print the shell prompt
static void shell_print_prompt(void)
{
    shell_print("\n> ");
}

// Command: help
// Command: cd - Change directory
static void cmd_cd(const char *args)
{
    if (!args || args[0] == '\0')
    {
        // cd without arguments - go to root
        current_dir[0] = '/';
        current_dir[1] = '\0';
        return;
    }

    // Normalize the path
    char normalized[256];
    normalize_path(args, normalized);

    // Check if directory exists by trying to open it
    extern struct inode *vfs_lookup_inode(const char *path);
    struct inode *inode = vfs_lookup_inode(normalized);

    if (!inode)
    {
        shell_print("\nError: Directory not found: ");
        shell_print(normalized);
        shell_print("\n");
        return;
    }

    // Check if it's a directory
    if (inode->type != VFS_DIRECTORY)
    {
        shell_print("\nError: Not a directory: ");
        shell_print(normalized);
        shell_print("\n");
        return;
    }

    // Update current directory
    strcpy(current_dir, normalized);
}

// Command: rm - Remove file
static void cmd_rm(const char *args)
{
    if (!args || args[0] == '\0')
    {
        shell_print("\nUsage: rm <file>\n");
        return;
    }

    // Normalize the path
    char normalized[256];
    normalize_path(args, normalized);

    shell_print("\nRemoving file: ");
    shell_print(normalized);
    shell_print("\n");

    // Call vfs_unlink
    extern int vfs_unlink(const char *path);
    int result = vfs_unlink(normalized);

    if (result == 0)
    {
        shell_print("File removed successfully.\n");
    }
    else
    {
        shell_print("Error: Failed to remove file.\n");
    }
}

static void cmd_help(void)
{
    shell_print("\nAvailable commands:\n");
    shell_print("  help    - Show this help message\n");
    shell_print("  clear   - Clear the screen\n");
    shell_print("  cd      - Change directory\n");
    shell_print("  echo    - Echo text to screen\n");
    shell_print("  about   - Show system information\n");
    shell_print("  mem     - Show memory information\n");
    shell_print("  page    - Test paging system\n");
    shell_print("  heap    - Show heap information\n");
    shell_print("  malloc  - Test memory allocation\n");
    shell_print("  ps      - Show current task\n");
    shell_print("  syscall - Test system calls\n");
    shell_print("  ls      - List files (usage: ls [path])\n");
    shell_print("  cat     - Display file contents\n");
    shell_print("  rm      - Remove file (usage: rm <file>)\n");
    shell_print("  devtest - Test device files\n");
    shell_print("  usertest - Test user mode execution\n");
    shell_print("  forktest - Test fork() system call\n");
    shell_print("  exectest - Test exec() system call\n");
    shell_print("  mkdir    - Create directory\n");
    shell_print("  rmdir    - Remove directory\n");
    shell_print("  schedtest - Test scheduler (MLFQ)\n");
    shell_print("  schedstop - Stop scheduler test\n");
    shell_print("  ipctest  - Test IPC (Inter-Process Communication)\n");
    shell_print("  ipcstop  - Stop IPC test\n");
    shell_print("  ipcinfo  - Show IPC statistics and ports\n");
    shell_print("  drvtest  - Test user-space driver (microkernel demo)\n");
    shell_print("  drvstop  - Stop driver test\n");
    shell_print("  iotest   - Test I/O port permissions and IRQ bridge\n");
    shell_print("  atadrv   - Start user-space ATA driver\n");
    shell_print("  netdrv   - Start user-space NE2000 driver\n");
    shell_print("  blktest  - Test block device IPC\n");
    shell_print("  net2ktest - Test network device IPC\n");
    shell_print("  mkfs     - Format a disk with VRFS\n");
    shell_print("  mount    - Show mounted filesystems\n");
    shell_print("  mount <dev> <path> - Mount a disk\n");
    shell_print("  umount   - Unmount a filesystem\n");
    shell_print("  lsblk    - List block devices\n");
    shell_print("  atatest  - Test ATA read/write\n");
    shell_print("  touch    - Create an empty file\n");
    shell_print("  write    - Write text to a file\n");
    shell_print("  ifconfig - Show network interfaces\n");
    shell_print("  nettest  - Test network packet send/receive\n");
}

// Command: about
static void cmd_about(void)
{
    shell_print("\nVROS - Virtual Real-time Operating System\n");
    shell_print("Version: 0.1.0\n");
    shell_print("A microkernel-based operating system\n");
    shell_print("Built with C and x86 assembly\n");
}

// Command: echo
static void cmd_echo(const char *args)
{
    shell_print("\n");
    shell_print(args);
}

// Helper function to convert number to string
static void int_to_str(uint32_t num, char *str)
{
    int i = 0;
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    int temp = num;
    int digits = 0;
    while (temp > 0)
    {
        digits++;
        temp /= 10;
    }

    i = digits - 1;
    while (num > 0)
    {
        str[i--] = '0' + (num % 10);
        num /= 10;
    }
    str[digits] = '\0';
}

// Command: mem
static void cmd_mem(void)
{
    char buffer[64];
    uint32_t total = pmm_get_memory_size();
    uint32_t used = pmm_get_used_blocks() * 4; // Convert to KB
    uint32_t free = pmm_get_free_blocks() * 4; // Convert to KB

    shell_print("\nMemory Information:\n");

    // Total memory
    shell_print("  Total:  ");
    int_to_str(total / 1024, buffer);
    shell_print(buffer);
    shell_print(" KB\n");

    // Used memory
    shell_print("  Used:   ");
    int_to_str(used, buffer);
    shell_print(buffer);
    shell_print(" KB\n");

    // Free memory
    shell_print("  Free:   ");
    int_to_str(free, buffer);
    shell_print(buffer);
    shell_print(" KB\n");
}

// Command: page
static void cmd_page(void)
{
    char buffer[64];

    shell_print("\nPaging System Test:\n");

    // Test some virtual to physical address translations
    void *test_addresses[] = {
        (void *)0x0,
        (void *)0x100000,
        (void *)0xB8000,
        0};

    for (int i = 0; test_addresses[i] != 0; i++)
    {
        void *virt = test_addresses[i];
        void *phys = paging_get_physical_address(virt);

        shell_print("  Virtual: 0x");
        int_to_str((uint32_t)virt, buffer);
        shell_print(buffer);
        shell_print(" -> Physical: 0x");
        int_to_str((uint32_t)phys, buffer);
        shell_print(buffer);
        shell_print("\n");
    }

    shell_print("Paging is active!\n");
}

// Command: heap
static void cmd_heap(void)
{
    char buffer[64];
    uint32_t total, used, free_mem;

    kmalloc_stats(&total, &used, &free_mem);

    shell_print("\nHeap Information:\n");

    // Total heap
    shell_print("  Total:  ");
    int_to_str(total / 1024, buffer);
    shell_print(buffer);
    shell_print(" KB\n");

    // Used heap
    shell_print("  Used:   ");
    int_to_str(used / 1024, buffer);
    shell_print(buffer);
    shell_print(" KB\n");

    // Free heap
    shell_print("  Free:   ");
    int_to_str(free_mem / 1024, buffer);
    shell_print(buffer);
    shell_print(" KB\n");
}

// Command: malloc (test memory allocation)
static void cmd_malloc_test(void)
{
    char buffer[64];

    shell_print("\nTesting kmalloc/kfree:\n");

    // Allocate some memory
    shell_print("  Allocating 128 bytes...\n");
    void *ptr1 = kmalloc(128);
    if (ptr1)
    {
        shell_print("  Success! Address: 0x");
        int_to_str((uint32_t)ptr1, buffer);
        shell_print(buffer);
        shell_print("\n");
    }
    else
    {
        shell_print("  Failed!\n");
    }

    // Allocate more memory
    shell_print("  Allocating 256 bytes...\n");
    void *ptr2 = kmalloc(256);
    if (ptr2)
    {
        shell_print("  Success! Address: 0x");
        int_to_str((uint32_t)ptr2, buffer);
        shell_print(buffer);
        shell_print("\n");
    }
    else
    {
        shell_print("  Failed!\n");
    }

    // Free first allocation
    shell_print("  Freeing first allocation...\n");
    kfree(ptr1);
    shell_print("  Done!\n");

    // Free second allocation
    shell_print("  Freeing second allocation...\n");
    kfree(ptr2);
    shell_print("  Done!\n");

    shell_print("Memory allocation test complete!\n");
}

// Command: ps
static void cmd_ps(void)
{
    char buffer[64];
    struct task *current = task_get_current();

    shell_print("\nCurrent Task: ");
    if (current)
    {
        shell_print(current->name);
        shell_print(" (PID ");
        int_to_str(current->pid, buffer);
        shell_print(buffer);
        shell_print(")\n");
    }
    else
    {
        shell_print("None\n");
    }

    shell_print("\nAll Tasks:\n");
    shell_print("PID  Name         State     Priority  CPU(ticks)  Switches\n");
    shell_print("---  -----------  --------  --------  ----------  --------\n");

    // Show all tasks
    for (int i = 0; i < 32; i++)
    {
        extern struct task *task_find_by_pid(int pid);
        struct task *t = task_find_by_pid(i);

        if (t)
        {
            // PID
            int_to_str(t->pid, buffer);
            shell_print(buffer);
            if (t->pid < 10)
                shell_print("    ");
            else
                shell_print("   ");

            // Name (truncate to 11 chars)
            int name_len = 0;
            while (t->name[name_len] && name_len < 11)
            {
                char ch[2] = {t->name[name_len], '\0'};
                shell_print(ch);
                name_len++;
            }
            while (name_len < 13)
            {
                shell_print(" ");
                name_len++;
            }

            // State
            switch (t->state)
            {
            case TASK_RUNNING:
                shell_print("RUNNING   ");
                break;
            case TASK_READY:
                shell_print("READY     ");
                break;
            case TASK_BLOCKED:
                shell_print("BLOCKED   ");
                break;
            case TASK_SLEEPING:
                shell_print("SLEEPING  ");
                break;
            case TASK_ZOMBIE:
                shell_print("ZOMBIE    ");
                break;
            }

            // Priority
            switch (t->priority)
            {
            case 0:
                shell_print("HIGH      ");
                break;
            case 1:
                shell_print("NORMAL    ");
                break;
            case 2:
                shell_print("LOW       ");
                break;
            case 3:
                shell_print("IDLE      ");
                break;
            default:
                shell_print("?         ");
                break;
            }

            // Total ticks
            int_to_str(t->total_ticks, buffer);
            shell_print(buffer);

            int tick_len = 0;
            while (buffer[tick_len])
                tick_len++;
            while (tick_len < 12)
            {
                shell_print(" ");
                tick_len++;
            }

            // Context switches
            int_to_str(t->context_switches, buffer);
            shell_print(buffer);

            shell_print("\n");
        }
    }
}

// Command: syscall
static void cmd_syscall_test(void)
{
    char buffer[64];

    shell_print("\nTesting System Calls:\n");

    // Test sys_getpid
    shell_print("  Calling sys_getpid()...\n");
    int pid;
    __asm__ volatile(
        "mov $3, %%eax\n"
        "int $0x80\n"
        "mov %%eax, %0"
        : "=r"(pid)
        :
        : "eax");
    shell_print("  PID: ");
    int_to_str(pid, buffer);
    shell_print(buffer);
    shell_print("\n");

    // Test sys_write
    shell_print("  Calling sys_write()...\n");
    const char *test_msg = "Hello from syscall!";
    int ret;
    __asm__ volatile(
        "mov $1, %%eax\n"
        "mov $1, %%ebx\n"
        "mov %1, %%ecx\n"
        "mov $19, %%edx\n"
        "int $0x80\n"
        "mov %%eax, %0"
        : "=r"(ret)
        : "r"(test_msg)
        : "eax", "ebx", "ecx", "edx");
    shell_print("  Bytes written: ");
    int_to_str(ret, buffer);
    shell_print(buffer);
    shell_print("\n");

    shell_print("System call test complete!\n");
}

// Command: ls helper - directly access ramfs
static void cmd_ls_dir(const char *path)
{
    shell_print("\nFiles in ");
    shell_print(path);
    shell_print(":\n");

    // Open directory using VFS
    struct file *f = vfs_open(path, 0);
    if (!f)
    {
        shell_print("  Error: Cannot access directory\n");
        return;
    }

    // Check if it's a directory
    if (f->inode->type != VFS_DIRECTORY)
    {
        shell_print("  Error: Not a directory\n");
        vfs_close(f);
        return;
    }

    // Access ramfs node directly
    struct ramfs_node
    {
        union
        {
            struct
            {
                char *data;
                uint32_t capacity;
                uint32_t size;
            };
            struct
            {
                void *entries;
                uint32_t num_entries;
            };
        };
    };

    struct ramfs_dirent
    {
        char name[256];
        struct inode *inode;
        void *next;
    };

    // Check if this is a VRFS directory (mounted filesystem)
    extern struct superblock *mount_get_sb(const char *path);

    // Normalize path: remove trailing slashes for mount check
    char normalized_path[256];
    int np = 0;
    for (int i = 0; path[i] && np < 255; i++)
        normalized_path[np++] = path[i];
    // Remove trailing slashes
    while (np > 1 && normalized_path[np - 1] == '/')
        np--;
    normalized_path[np] = '\0';

    struct superblock *mounted_sb = mount_get_sb(normalized_path);

    if (mounted_sb)
    {
        // This is a mounted VRFS

        // VRFS directory entry structure
        struct vrfs_dirent
        {
            uint32_t inode;
            char name[28];
        };

        // Use the correct vrfs_inode_info from vrfs.h
        struct vrfs_inode_info *dir_info = (struct vrfs_inode_info *)f->inode->private_data;
        if (!dir_info)
        {
            shell_print("  Error: No directory info\n");
            vfs_close(f);
            return;
        }

        // Get VRFS sb_info to read fresh inode from disk (use definition from vrfs.h)
        struct vrfs_sb_info *sbi = (struct vrfs_sb_info *)mounted_sb->private_data;
        if (sbi)
        {
            // Read fresh inode data from disk to ensure consistency
            extern int vrfs_read_inode(struct vrfs_sb_info * sbi, uint32_t inode_no, void *inode_data);
            vrfs_read_inode(sbi, dir_info->inode_no, &dir_info->disk_inode);
        }

        // Check if directory has data block
        if (dir_info->disk_inode.direct[0] == 0)
        {
            shell_print("  (empty - no data block)\n");
            vfs_close(f);
            return;
        }

        // Read directory block
        uint8_t *block_buf = (uint8_t *)kmalloc(512);
        if (!block_buf)
        {
            shell_print("  Error: Memory allocation failed\n");
            vfs_close(f);
            return;
        }

        // Get block device
        extern struct block_device *blkdev_get(const char *name);
        struct block_device *bdev = blkdev_get("hda");
        if (!bdev)
        {
            shell_print("  Error: Cannot access disk\n");
            kfree(block_buf);
            vfs_close(f);
            return;
        }

        // Read directory data block
        if (blkdev_read(bdev, dir_info->disk_inode.direct[0], block_buf) < 0)
        {
            shell_print("  Error: Cannot read directory\n");
            kfree(block_buf);
            vfs_close(f);
            return;
        }

        // List entries
        struct vrfs_dirent *entries = (struct vrfs_dirent *)block_buf;
        int max_entries = 512 / sizeof(struct vrfs_dirent);
        int count = 0;

        for (int i = 0; i < max_entries; i++)
        {
            if (entries[i].inode == 0)
                continue;

            shell_print("  [FILE] ");
            shell_print(entries[i].name);
            shell_print("\n");
            count++;
        }

        kfree(block_buf);

        if (count == 0)
        {
            shell_print("  (empty)\n");
        }
        else
        {
            shell_print("\nTotal: ");
            char count_str[16];
            int_to_str(count, count_str);
            shell_print(count_str);
            shell_print(" item(s)\n");
        }
    }
    else
    {
        // This is a ramfs directory
        struct ramfs_node *node = (struct ramfs_node *)f->inode->private_data;
        if (!node)
        {
            shell_print("  Error: Invalid directory\n");
            vfs_close(f);
            return;
        }

        // List entries
        struct ramfs_dirent *entry = (struct ramfs_dirent *)node->entries;
        int count = 0;
        int max_entries = 1000; // Safety limit to prevent infinite loops

        while (entry && count < max_entries)
        {
            shell_print("  ");

            // Show type indicator
            if (entry->inode && entry->inode->type == VFS_DIRECTORY)
            {
                shell_print("[DIR]  ");
            }
            else
            {
                shell_print("[FILE] ");
            }

            shell_print(entry->name);

            // Show file size for files only
            if (entry->inode && entry->inode->type != VFS_DIRECTORY)
            {
                shell_print(" (");
                char size_str[16];
                int_to_str(entry->inode->size, size_str);
                shell_print(size_str);
                shell_print(" bytes)");
            }

            shell_print("\n");
            entry = (struct ramfs_dirent *)entry->next;
            count++;
        }

        if (count >= max_entries)
        {
            shell_print("  Warning: Directory listing truncated (too many entries or loop detected)\n");
        }

        if (count == 0)
        {
            shell_print("  (empty)\n");
        }
        else
        {
            shell_print("\nTotal: ");
            char count_str[16];
            int_to_str(count, count_str);
            shell_print(count_str);
            shell_print(" item(s)\n");
        }
    }

    vfs_close(f);
}

// Command: ls
static void cmd_ls(const char *args)
{
    // Parse argument
    if (args && args[0] != '\0')
    {
        // Skip leading spaces
        while (*args == ' ')
            args++;

        if (*args != '\0')
        {
            // Normalize the path
            char normalized[256];
            normalize_path(args, normalized);
            cmd_ls_dir(normalized);
            return;
        }
    }

    // Default: list current directory
    cmd_ls_dir(current_dir);
}

// Command: cat
static void cmd_cat(const char *filename)
{
    // Normalize the path
    char normalized[256];
    normalize_path(filename, normalized);

    // Open file
    struct file *f = vfs_open(normalized, 0);
    if (!f)
    {
        shell_print("\nError: Cannot open file '");
        shell_print(normalized);
        shell_print("'\n");
        return;
    }

    // Read and display content
    shell_print("\n");
    char buffer[512];
    int bytes_read = vfs_read(f, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        shell_print(buffer);
    }
    else
    {
        shell_print("(empty file)\n");
    }

    // Close file
    vfs_close(f);
}

// Command: devtest
static void cmd_devtest(void)
{
    shell_print("\nTesting device files:\n");

    // Test /dev/null
    shell_print("\n1. Testing /dev/null:\n");
    struct file *null_file = vfs_open("/dev/null", 0);
    if (null_file)
    {
        shell_print("   Writing to /dev/null... ");
        const char *test_data = "This should disappear";
        int written = vfs_write(null_file, test_data, 21);
        shell_print("Wrote ");
        char buf[16];
        int_to_str(written, buf);
        shell_print(buf);
        shell_print(" bytes\n");

        shell_print("   Reading from /dev/null... ");
        char read_buf[32];
        int read_bytes = vfs_read(null_file, read_buf, 32);
        shell_print("Read ");
        int_to_str(read_bytes, buf);
        shell_print(buf);
        shell_print(" bytes (should be 0)\n");

        vfs_close(null_file);
    }
    else
    {
        shell_print("   Error: Cannot open /dev/null\n");
    }

    // Test /dev/zero
    shell_print("\n2. Testing /dev/zero:\n");
    struct file *zero_file = vfs_open("/dev/zero", 0);
    if (zero_file)
    {
        shell_print("   Reading 16 bytes from /dev/zero:\n   ");
        char zero_buf[16];
        int read_bytes = vfs_read(zero_file, zero_buf, 16);

        for (int i = 0; i < read_bytes; i++)
        {
            if (zero_buf[i] == 0)
            {
                shell_print("0 ");
            }
            else
            {
                shell_print("? ");
            }
        }
        shell_print("\n");

        vfs_close(zero_file);
    }
    else
    {
        shell_print("   Error: Cannot open /dev/zero\n");
    }

    // Test /dev/random
    shell_print("\n3. Testing /dev/random:\n");
    struct file *random_file = vfs_open("/dev/random", 0);
    if (random_file)
    {
        shell_print("   Reading 8 bytes from /dev/random:\n   ");
        unsigned char random_buf[8];
        int read_bytes = vfs_read(random_file, (char *)random_buf, 8);

        for (int i = 0; i < read_bytes; i++)
        {
            char hex[4];
            unsigned char val = random_buf[i];
            hex[0] = "0123456789ABCDEF"[val >> 4];
            hex[1] = "0123456789ABCDEF"[val & 0xF];
            hex[2] = ' ';
            hex[3] = '\0';
            shell_print(hex);
        }
        shell_print("\n");

        vfs_close(random_file);
    }
    else
    {
        shell_print("   Error: Cannot open /dev/random\n");
    }

    shell_print("\nDevice test complete!\n");
}

// External user program functions
extern void *get_user_program_test(void);
extern uint32_t get_user_program_test_size(void);

// Command: usertest
static void cmd_usertest(void)
{
    shell_print("\nTesting user mode execution:\n");
    shell_print("  Note: User mode is a complex feature.\n");
    shell_print("  This is a simplified demonstration.\n\n");

    shell_print("  Current privilege level: Ring 0 (Kernel)\n");
    shell_print("  Attempting to switch to Ring 3 (User)...\n\n");

    // Get user program
    void *user_prog = get_user_program_test();
    uint32_t prog_size = get_user_program_test_size();

    shell_print("  User program loaded at: 0x");
    char addr_str[16];
    uint32_t addr = (uint32_t)user_prog;
    for (int i = 7; i >= 0; i--)
    {
        addr_str[7 - i] = "0123456789ABCDEF"[(addr >> (i * 4)) & 0xF];
    }
    addr_str[8] = '\0';
    shell_print(addr_str);
    shell_print("\n");

    shell_print("  Program size: ");
    char size_str[16];
    int_to_str(prog_size, size_str);
    shell_print(size_str);
    shell_print(" bytes\n\n");

    shell_print("  Status: User mode support initialized\n");
    shell_print("  (Full user mode execution requires more setup)\n");

    shell_print("\nUser mode test complete!\n");
}

// Simple test task that will be created (not forked)
static void simple_test_child_task(void)
{
    shell_print("  Child task running!\n");

    // Get current task info
    struct task *current = task_get_current();
    char pid_str[16];

    shell_print("  Child PID: ");
    int_to_str(current->pid, pid_str);
    shell_print(pid_str);

    if (current->parent)
    {
        shell_print(", Parent PID: ");
        int_to_str(current->parent->pid, pid_str);
        shell_print(pid_str);
    }
    shell_print("\n");

    // Exit with status 42
    shell_print("  Child exiting with status 42\n");
    task_exit(42);
}

// Command: forktest - uses task_create for testing
static void cmd_forktest(void)
{
    shell_print("\nTesting fork() system call framework:\n");
    shell_print("  Note: Direct fork() from kernel is complex.\n");
    shell_print("  Using task_create to test process lifecycle.\n\n");

    struct task *current = task_get_current();
    char pid_str[16];

    shell_print("  Parent PID: ");
    int_to_str(current->pid, pid_str);
    shell_print(pid_str);
    shell_print("\n");

    // Create a child task (simulating fork behavior)
    shell_print("  Creating child task...\n");
    uint32_t child_pid = task_create("fork_test_child", simple_test_child_task);

    if (child_pid == 0)
    {
        shell_print("  Error: task creation failed!\n");
        return;
    }

    shell_print("  Child task created with PID: ");
    int_to_str(child_pid, pid_str);
    shell_print(pid_str);
    shell_print("\n");

    // Give child time to run
    shell_print("  Yielding to let child run...\n");
    for (int i = 0; i < 10; i++)
    {
        task_yield();
    }

    // Wait for child
    shell_print("  Waiting for child to exit...\n");
    int status = 0;
    int result = task_waitpid(child_pid, &status);

    if (result > 0)
    {
        shell_print("  Child exited with status: ");
        int_to_str(status, pid_str);
        shell_print(pid_str);
        shell_print("\n");
    }
    else if (result == -2)
    {
        shell_print("  Child still running\n");
    }
    else
    {
        shell_print("  waitpid() failed\n");
    }

    shell_print("\nFork test complete!\n");
    shell_print("  fork() infrastructure is ready for userspace programs.\n");
}

// Command: mkdir
static void cmd_mkdir(const char *path_arg)
{
    if (!path_arg || *path_arg == '\0')
    {
        shell_print("\nUsage: mkdir <directory>\n");
        return;
    }

    // Skip leading spaces
    while (*path_arg == ' ')
        path_arg++;

    if (*path_arg == '\0')
    {
        shell_print("\nUsage: mkdir <directory>\n");
        return;
    }

    // Build path
    char path[256];
    int i = 0;

    // Add leading slash if not present
    if (*path_arg != '/')
    {
        path[0] = '/';
        i = 1;
    }

    while (*path_arg && *path_arg != ' ' && i < 255)
    {
        path[i++] = *path_arg++;
    }
    path[i] = '\0';

    shell_print("\nCreating directory: ");
    shell_print(path);
    shell_print("\n");

    // Create directory
    if (vfs_mkdir(path, 0755) == 0)
    {
        shell_print("  Success!\n");
    }
    else
    {
        shell_print("  Error: Failed to create directory\n");
        shell_print("  (Check if parent exists or name is valid)\n");
    }
}

// Command: rmdir
static void cmd_rmdir(const char *path_arg)
{
    if (!path_arg || *path_arg == '\0')
    {
        shell_print("\nUsage: rmdir <directory>\n");
        return;
    }

    // Skip leading spaces
    while (*path_arg == ' ')
        path_arg++;

    if (*path_arg == '\0')
    {
        shell_print("\nUsage: rmdir <directory>\n");
        return;
    }

    // Build path
    char path[256];
    int i = 0;

    // Add leading slash if not present
    if (*path_arg != '/')
    {
        path[0] = '/';
        i = 1;
    }

    while (*path_arg && *path_arg != ' ' && i < 255)
    {
        path[i++] = *path_arg++;
    }
    path[i] = '\0';

    shell_print("\nRemoving directory: ");
    shell_print(path);
    shell_print("\n");

    // Remove directory
    if (vfs_rmdir(path) == 0)
    {
        shell_print("  Success!\n");
    }
    else
    {
        shell_print("  Error: Failed to remove directory\n");
        shell_print("  (Directory must be empty or may not exist)\n");
    }
}

// Command: schedtest
static void cmd_schedtest(void)
{
    shell_print("\nStarting Scheduler Test (Minimal)...\n");
    shell_print("Creating 2 minimal test tasks:\n");
    shell_print("  1. test1 - Counts to 1M then exits\n");
    shell_print("  2. test2 - Counts to 500K then exits\n\n");
    shell_print("NO infinite loops, NO yield, NO priority changes.\n");
    shell_print("Tasks will run briefly and finish.\n\n");
    shell_print("Use 'ps' to see if they ran.\n");

    extern void sched_test_create_tasks(void);
    sched_test_create_tasks();

    shell_print("\nTest tasks created!\n");
    shell_print("Wait a moment, then run 'ps' to check.\n");
}

// Command: schedstop
static void cmd_schedstop(void)
{
    shell_print("\nStopping scheduler test tasks...\n");

    extern void sched_test_stop_tasks(void);
    sched_test_stop_tasks();

    shell_print("Test tasks stopped.\n");
}

// Command: ipctest
static void cmd_ipctest(void)
{
    shell_print("\nStarting IPC Test...\n");
    shell_print("Creating IPC server and client tasks:\n");
    shell_print("  1. ipc_server - Creates named port 'echo_service'\n");
    shell_print("  2. ipc_client - Sends messages to server\n\n");
    shell_print("Tasks run continuously until you run 'ipcstop'\n");
    shell_print("\nUse these commands:\n");
    shell_print("  ps      - See task status and CPU usage\n");
    shell_print("  ipcinfo - See IPC ports and statistics\n");
    shell_print("  ipcstop - Stop the test\n");

    extern void ipc_test_start(void);
    ipc_test_start();

    shell_print("\nIPC test started! Run 'ipcinfo' to see ports.\n");
}

// Command: ipcstop
static void cmd_ipcstop(void)
{
    shell_print("\nStopping IPC Test...\n");
    shell_print("Marking IPC tasks as ZOMBIE.\n");

    extern void ipc_test_stop(void);
    ipc_test_stop();

    shell_print("Test stopped! Use 'ps' to verify.\n");
}

// Command: ipcinfo
static void cmd_ipcinfo(void)
{
    extern void ipc_get_stats(void *);
    extern struct ipc_port *ipc_get_port(uint32_t);

    struct
    {
        uint32_t total_ports;
        uint32_t active_ports;
        uint32_t total_messages;
        uint32_t blocked_tasks;
    } stats;

    ipc_get_stats(&stats);

    shell_print("\n=== IPC Statistics ===\n");
    shell_print("Total Ports:     ");
    char buf[16];
    int_to_str(stats.total_ports, buf);
    shell_print(buf);
    shell_print("\n");

    shell_print("Active Ports:    ");
    int_to_str(stats.active_ports, buf);
    shell_print(buf);
    shell_print("\n");

    shell_print("Total Messages:  ");
    int_to_str(stats.total_messages, buf);
    shell_print(buf);
    shell_print("\n");

    shell_print("Blocked Tasks:   ");
    int_to_str(stats.blocked_tasks, buf);
    shell_print(buf);
    shell_print("\n");

    if (stats.active_ports > 0)
    {
        shell_print("\n=== Active Ports ===\n");
        for (int i = 0; i < 32; i++)
        {
            void *port = ipc_get_port(i);
            if (port)
            {
                // Port structure access
                uint32_t *port_data = (uint32_t *)port;
                uint32_t port_id = port_data[0];
                uint32_t owner_pid = port_data[1];
                // uint32_t in_use = port_data[2];
                char *name = (char *)&port_data[3];
                uint32_t queue_count = port_data[19]; // After name[32]
                uint32_t total_sent = port_data[20];
                uint32_t total_received = port_data[21];
                uint32_t drops = port_data[22];

                shell_print("Port ");
                int_to_str(port_id, buf);
                shell_print(buf);
                shell_print(": Owner=");
                int_to_str(owner_pid, buf);
                shell_print(buf);

                if (name[0] != '\0')
                {
                    shell_print(" Name=\"");
                    shell_print(name);
                    shell_print("\"");
                }

                shell_print(" Queue=");
                int_to_str(queue_count, buf);
                shell_print(buf);
                shell_print("/16");

                shell_print(" Sent=");
                int_to_str(total_sent, buf);
                shell_print(buf);

                shell_print(" Recv=");
                int_to_str(total_received, buf);
                shell_print(buf);

                if (drops > 0)
                {
                    shell_print(" Drops=");
                    int_to_str(drops, buf);
                    shell_print(buf);
                }

                shell_print("\n");
            }
        }
    }
}

// Command: drvtest
static void cmd_drvtest(void)
{
    shell_print("\n=== User-Space Driver Test ===\n");
    shell_print("Demonstrating Microkernel Architecture!\n\n");
    shell_print("Creating:\n");
    shell_print("  1. kbd_driver - Keyboard driver in USER SPACE\n");
    shell_print("  2. kbd_client1 - Application client 1\n");
    shell_print("  3. kbd_client2 - Application client 2\n\n");
    shell_print("How it works:\n");
    shell_print("  • Driver creates named port 'kbd_driver'\n");
    shell_print("  • Clients register with driver via IPC\n");
    shell_print("  • Driver broadcasts key events to clients\n");
    shell_print("  • All communication via IPC (no kernel calls!)\n\n");
    shell_print("Use 'ipcinfo' to see the driver port and messages.\n");
    shell_print("Use 'ps' to see all tasks running.\n");
    shell_print("Use 'drvstop' to stop the test.\n");

    extern void userspace_driver_start(void);
    userspace_driver_start();

    shell_print("\nUser-space driver started!\n");
}

// Command: drvstop
static void cmd_drvstop(void)
{
    shell_print("\nStopping user-space driver test...\n");

    extern void userspace_driver_stop(void);
    userspace_driver_stop();

    shell_print("Driver test stopped!\n");
}

// Command: blktest - Test block device IPC
static void cmd_blktest(void)
{
    shell_print("\n=== Block Device IPC Test ===\n\n");

    extern int blkdev_ipc_driver_available(void);
    extern int blkdev_ipc_read(uint8_t drive, uint32_t lba, uint32_t count, void *buffer);
    extern int blkdev_ipc_write(uint8_t drive, uint32_t lba, uint32_t count, const void *buffer);
    extern int blkdev_ipc_client_init(void);

    // 检查驱动是否可用
    shell_print("Checking if driver is available...\n");
    if (!blkdev_ipc_driver_available())
    {
        shell_print("[FAIL] ATA driver is not running!\n");
        shell_print("Run 'atadrv' first to start the driver.\n\n");
        return;
    }
    shell_print("[OK] Driver is available\n\n");

    // 分配测试缓冲区
    extern void *kmalloc(uint32_t size);
    extern void kfree(void *ptr);

    uint8_t *buffer = (uint8_t *)kmalloc(512);
    if (!buffer)
    {
        shell_print("[FAIL] Failed to allocate buffer\n\n");
        return;
    }

    // 测试 1: 读取扇区 0
    shell_print("Test 1: Reading sector 0 via IPC...\n");
    int result = blkdev_ipc_read(0, 0, 1, buffer);
    if (result > 0)
    {
        shell_print("[OK] Read ");
        char num[16];
        int_to_str(result, num);
        shell_print(num);
        shell_print(" bytes\n");

        // 显示前 16 个字节
        shell_print("First 16 bytes: ");
        for (int i = 0; i < 16; i++)
        {
            char hex[4];
            uint8_t b = buffer[i];
            hex[0] = "0123456789ABCDEF"[b >> 4];
            hex[1] = "0123456789ABCDEF"[b & 0xF];
            hex[2] = ' ';
            hex[3] = '\0';
            shell_print(hex);
        }
        shell_print("\n");
    }
    else
    {
        shell_print("[FAIL] Read failed\n");
    }

    // 测试 2: 写入测试
    shell_print("\nTest 2: Write/Read test...\n");
    // 准备测试数据
    for (int i = 0; i < 512; i++)
    {
        buffer[i] = (uint8_t)(i & 0xFF);
    }

    // 写入到扇区 1
    result = blkdev_ipc_write(0, 1, 1, buffer);
    if (result > 0)
    {
        shell_print("[OK] Written ");
        char num[16];
        int_to_str(result, num);
        shell_print(num);
        shell_print(" bytes\n");

        // 清空缓冲区
        for (int i = 0; i < 512; i++)
            buffer[i] = 0;

        // 读回
        result = blkdev_ipc_read(0, 1, 1, buffer);
        if (result > 0)
        {
            shell_print("[OK] Read back ");
            int_to_str(result, num);
            shell_print(num);
            shell_print(" bytes\n");

            // 验证数据
            int errors = 0;
            for (int i = 0; i < 512; i++)
            {
                if (buffer[i] != (uint8_t)(i & 0xFF))
                    errors++;
            }

            if (errors == 0)
            {
                shell_print("[OK] Data verification passed!\n");
            }
            else
            {
                shell_print("[FAIL] Data verification failed (");
                int_to_str(errors, num);
                shell_print(num);
                shell_print(" errors)\n");
            }
        }
        else
        {
            shell_print("[FAIL] Read back failed\n");
        }
    }
    else
    {
        shell_print("[FAIL] Write failed\n");
    }

    kfree(buffer);

    shell_print("\n=== Test completed! ===\n\n");
}

// Command: atadrv - Start user-space ATA driver
static void cmd_atadrv(void)
{
    shell_print("\n=== Starting User-Space ATA Driver ===\n\n");

    extern void ata_driver_main(void);
    extern uint32_t task_create(const char *name, void (*entry_point)(void));

    uint32_t drv_pid = task_create("ata_driver", ata_driver_main);
    if (drv_pid > 0)
    {
        shell_print("[OK] ATA driver task created (PID: ");
        char pid_str[16];
        int_to_str(drv_pid, pid_str);
        shell_print(pid_str);
        shell_print(")\n");
        shell_print("Use 'ps' to check driver status.\n\n");
    }
    else
    {
        shell_print("[FAIL] Failed to create driver task\n\n");
    }
}

// Command: netdrv - Start NE2000 driver
static void cmd_netdrv(void)
{
    shell_print("\n=== Starting User-Space NE2000 Driver ===\n\n");

    extern void ne2000_driver_main(void);
    extern uint32_t task_create(const char *name, void (*entry_point)(void));

    uint32_t drv_pid = task_create("ne2000_driver", ne2000_driver_main);
    if (drv_pid > 0)
    {
        shell_print("[OK] NE2000 driver task created (PID: ");
        char pid_str[16];
        int_to_str(drv_pid, pid_str);
        shell_print(pid_str);
        shell_print(")\n");
        shell_print("Use 'ps' to check driver status.\n\n");
    }
    else
    {
        shell_print("[FAIL] Failed to create driver task\n\n");
    }
}

// Command: net2ktest - Test NE2000 IPC
static void cmd_net2ktest(void)
{
    shell_print("\n=== Network Device IPC Test ===\n\n");

    extern int netdev_ipc_driver_available(void);
    extern int netdev_ipc_get_mac(uint8_t *mac);
    extern int netdev_ipc_send(const uint8_t *data, uint32_t length);

    // 检查驱动是否可用
    shell_print("Checking if driver is available...\n");
    
    extern int ipc_find_port(const char *name);
    int port_id = ipc_find_port("netdev.ne2000");
    
    shell_print("Debug: ipc_find_port returned: ");
    char port_str[16];
    int_to_str(port_id, port_str);
    shell_print(port_str);
    shell_print("\n");
    
    if (!netdev_ipc_driver_available())
    {
        shell_print("[FAIL] NE2000 driver is not running!\n");
        shell_print("Run 'netdrv' first to start the driver.\n\n");
        return;
    }
    shell_print("[OK] Driver is available\n\n");

    // 获取 MAC 地址
    shell_print("Getting MAC address...\n");
    uint8_t mac[6];
    if (netdev_ipc_get_mac(mac) == 0)
    {
        shell_print("[OK] MAC: ");
        for (int i = 0; i < 6; i++)
        {
            char hex[4];
            uint8_t b = mac[i];
            hex[0] = "0123456789ABCDEF"[b >> 4];
            hex[1] = "0123456789ABCDEF"[b & 0xF];
            hex[2] = (i < 5) ? ':' : '\0';
            hex[3] = '\0';
            shell_print(hex);
        }
        shell_print("\n\n");
    }
    else
    {
        shell_print("[FAIL] Failed to get MAC address\n\n");
    }

    // 测试发送（发送一个简单的以太网帧）
    shell_print("Test: Sending packet...\n");
    uint8_t test_packet[60];
    // 目标 MAC: FF:FF:FF:FF:FF:FF (广播)
    for (int i = 0; i < 6; i++)
        test_packet[i] = 0xFF;
    // 源 MAC: 使用获取的 MAC
    for (int i = 0; i < 6; i++)
        test_packet[6 + i] = mac[i];
    // EtherType: 0x0800 (IP)
    test_packet[12] = 0x08;
    test_packet[13] = 0x00;
    // 填充数据
    for (int i = 14; i < 60; i++)
        test_packet[i] = i;

    int result = netdev_ipc_send(test_packet, 60);
    if (result > 0)
    {
        shell_print("[OK] Sent ");
        char num[16];
        int_to_str(result, num);
        shell_print(num);
        shell_print(" bytes\n");
    }
    else
    {
        shell_print("[FAIL] Send failed\n");
    }

    shell_print("\n=== Test completed! ===\n\n");
}

// Command: iotest - Test I/O port and IRQ bridge
static void cmd_iotest(void)
{
    shell_print("\n=== Microkernel I/O & IRQ Test Suite ===\n");

    // 测试 1: I/O 权限系统（内核级测试）
    shell_print("\n=== Test 1: I/O Permission System ===\n");

    extern int ioport_grant_access(uint16_t port_start, uint16_t port_end);
    extern int ioport_check_access(uint16_t port);

    shell_print("Granting access to serial port (0x3F8-0x3FF)...\n");
    int result = ioport_grant_access(0x3F8, 0x3FF);

    if (result == 0)
    {
        shell_print("[OK] Permission granted!\n");

        // 检查权限
        if (ioport_check_access(0x3F8))
        {
            shell_print("[OK] Permission check passed for 0x3F8\n");
        }
        else
        {
            shell_print("[FAIL] Permission check failed for 0x3F8\n");
        }
    }
    else
    {
        shell_print("[FAIL] Failed to grant permission\n");
    }

    // 测试 2: IRQ 桥接系统
    shell_print("\n=== Test 2: IRQ Bridge System ===\n");

    extern int ipc_create_port(void);
    extern int irq_bridge_register(uint8_t irq, uint32_t ipc_port);

    shell_print("Creating IPC port...\n");
    int port = ipc_create_port();

    if (port >= 0)
    {
        shell_print("[OK] IPC port created: ");
        char num[16];
        int_to_str(port, num);
        shell_print(num);
        shell_print("\n");

        shell_print("Registering keyboard IRQ handler (IRQ 1)...\n");
        result = irq_bridge_register(1, port);

        if (result == 0)
        {
            shell_print("[OK] IRQ handler registered!\n");
            shell_print("Note: IRQ messages will be sent to port ");
            int_to_str(port, num);
            shell_print(num);
            shell_print(" on keyboard events\n");
        }
        else
        {
            shell_print("[FAIL] Failed to register IRQ handler\n");
        }
    }
    else
    {
        shell_print("[FAIL] Failed to create IPC port\n");
    }

    shell_print("\n=== Tests completed! ===\n");
    shell_print("\nNote: These are kernel-level tests.\n");
    shell_print("For full user-space testing, user-space drivers\n");
    shell_print("need to be implemented.\n\n");
}

// Command: lsblk - List block devices
static void cmd_lsblk(void)
{
    shell_print("\nBlock Devices:\n");
    shell_print("NAME       SIZE(MB)   STATUS\n");
    shell_print("------------------------------------\n");

    for (int i = 0; i < 4; i++)
    {
        struct ata_device *dev = ata_get_device(i);
        if (dev && dev->exists)
        {
            char buffer[64];

            // Device name
            shell_print("hd");
            buffer[0] = 'a' + i;
            buffer[1] = '\0';
            shell_print(buffer);
            shell_print("        ");

            // Size in MB
            uint32_t size_mb = (dev->size / 2048); // sectors to MB
            int_to_str(size_mb, buffer);
            shell_print(buffer);
            shell_print("        ");

            // Status
            shell_print("Ready\n");
        }
    }
}

// Command: atatest - Test ATA read/write
static void cmd_atatest(const char *args)
{
    (void)args;

    shell_print("\n=== ATA Read/Write Test ===\n");

    // Get hda
    extern struct block_device *blkdev_get(const char *name);
    struct block_device *bdev = blkdev_get("hda");
    if (!bdev)
    {
        shell_print("ERROR: hda not found!\n");
        return;
    }

    shell_print("Found hda, testing...\n");

    // Allocate test buffers
    uint8_t *write_buf = (uint8_t *)kmalloc(512);
    uint8_t *read_buf = (uint8_t *)kmalloc(512);

    if (!write_buf || !read_buf)
    {
        shell_print("ERROR: Buffer allocation failed!\n");
        if (write_buf)
            kfree(write_buf);
        if (read_buf)
            kfree(read_buf);
        return;
    }

    // Fill write buffer with zeros
    for (int i = 0; i < 512; i++)
        write_buf[i] = 0;

    // Write test pattern
    ((uint32_t *)write_buf)[0] = 0x12345678;
    ((uint32_t *)write_buf)[1] = 0xABCDEF01;
    ((uint32_t *)write_buf)[2] = 0xDEADBEEF;
    ((uint32_t *)write_buf)[3] = 0xCAFEBABE;

    shell_print("Write pattern:\n  0x12345678 0xABCDEF01\n  0xDEADBEEF 0xCAFEBABE\n");

    // Write
    shell_print("Writing...");
    if (blkdev_write(bdev, 0, write_buf) < 0)
    {
        shell_print(" FAILED!\n");
        kfree(write_buf);
        kfree(read_buf);
        return;
    }
    shell_print(" OK\n");

    // Clear read buffer
    for (int i = 0; i < 512; i++)
        read_buf[i] = 0xFF;

    // Read
    shell_print("Reading...");
    if (blkdev_read(bdev, 0, read_buf) < 0)
    {
        shell_print(" FAILED!\n");
        kfree(write_buf);
        kfree(read_buf);
        return;
    }
    shell_print(" OK\n");

    // Print result
    shell_print("Read back:\n  ");
    char msg[16];
    const char *hex = "0123456789ABCDEF";
    for (int j = 0; j < 4; j++)
    {
        uint32_t val = ((uint32_t *)read_buf)[j];
        msg[0] = '0';
        msg[1] = 'x';
        for (int i = 7; i >= 0; i--)
            msg[2 + (7 - i)] = hex[(val >> (i * 4)) & 0xF];
        msg[10] = ' ';
        msg[11] = '\0';
        shell_print(msg);
        if (j == 1)
            shell_print("\n  ");
    }
    shell_print("\n");

    // Compare
    int mismatches = 0;
    for (int i = 0; i < 512; i++)
    {
        if (write_buf[i] != read_buf[i])
            mismatches++;
    }

    if (mismatches == 0)
    {
        shell_print("Result: SUCCESS (all 512 bytes match)\n");
    }
    else
    {
        shell_print("Result: FAILED (");
        char num[16];
        int_to_str(mismatches, num);
        shell_print(num);
        shell_print(" bytes differ)\n");
    }

    kfree(write_buf);
    kfree(read_buf);
}

// Command: ifconfig - Show network interfaces
static void cmd_ifconfig(const char *args)
{
    (void)args;

    shell_print("\n=== Network Interfaces ===\n");

    // Get network interface
    extern struct netif *netif_get(const char *name);
    struct netif *netif = netif_get("eth0");

    if (!netif)
    {
        shell_print("No network interfaces found.\n");
        return;
    }

    shell_print("Interface: ");
    shell_print(netif->name);
    shell_print("\n");

    // Print MAC address
    shell_print("  MAC Address: ");
    const char *hex = "0123456789ABCDEF";
    char mac_str[3];
    for (int i = 0; i < 6; i++)
    {
        mac_str[0] = hex[(netif->mac_addr[i] >> 4) & 0xF];
        mac_str[1] = hex[netif->mac_addr[i] & 0xF];
        mac_str[2] = '\0';
        shell_print(mac_str);
        if (i < 5)
            shell_print(":");
    }
    shell_print("\n");

    // Print statistics
    shell_print("  TX packets: ");
    char num[32];
    int_to_str(netif->stats.packets_sent, num);
    shell_print(num);
    shell_print("  bytes: ");
    int_to_str(netif->stats.bytes_sent, num);
    shell_print(num);
    shell_print("\n");

    shell_print("  RX packets: ");
    int_to_str(netif->stats.packets_received, num);
    shell_print(num);
    shell_print("  bytes: ");
    int_to_str(netif->stats.bytes_received, num);
    shell_print(num);
    shell_print("\n");

    shell_print("  Errors: ");
    int_to_str(netif->stats.errors, num);
    shell_print(num);
    shell_print("\n");
}

// Command: nettest - Test network send/receive
static void cmd_nettest(const char *args)
{
    (void)args;

    shell_print("\n=== Network Packet Test ===\n");

    // Get network interface
    extern struct netif *netif_get(const char *name);
    extern int netif_send(struct netif * netif, const uint8_t *data, uint16_t length);
    extern int netif_receive(struct netif * netif, uint8_t *buffer, uint16_t max_length);

    struct netif *netif = netif_get("eth0");
    if (!netif)
    {
        shell_print("ERROR: eth0 not found!\n");
        return;
    }

    shell_print("Found eth0, creating test packet...\n");

    // Create a test Ethernet frame (broadcast ARP request)
    uint8_t packet[60]; // Minimum Ethernet frame size

    // Destination MAC (broadcast)
    for (int i = 0; i < 6; i++)
        packet[i] = 0xFF;

    // Source MAC (from our interface)
    for (int i = 0; i < 6; i++)
        packet[6 + i] = netif->mac_addr[i];

    // EtherType (ARP = 0x0806)
    packet[12] = 0x08;
    packet[13] = 0x06;

    // ARP Header
    packet[14] = 0x00;
    packet[15] = 0x01; // Hardware type: Ethernet
    packet[16] = 0x08;
    packet[17] = 0x00; // Protocol type: IPv4
    packet[18] = 0x06; // Hardware size
    packet[19] = 0x04; // Protocol size
    packet[20] = 0x00;
    packet[21] = 0x01; // Opcode: Request

    // Sender MAC
    for (int i = 0; i < 6; i++)
        packet[22 + i] = netif->mac_addr[i];

    // Sender IP (192.168.1.100)
    packet[28] = 192;
    packet[29] = 168;
    packet[30] = 1;
    packet[31] = 100;

    // Target MAC (zeros for request)
    for (int i = 0; i < 6; i++)
        packet[32 + i] = 0x00;

    // Target IP (192.168.1.1)
    packet[38] = 192;
    packet[39] = 168;
    packet[40] = 1;
    packet[41] = 1;

    // Pad to minimum size
    for (int i = 42; i < 60; i++)
        packet[i] = 0x00;

    shell_print("Sending ARP request...\n");

    if (netif_send(netif, packet, 60) == 0)
    {
        shell_print("Packet sent successfully!\n");
    }
    else
    {
        shell_print("ERROR: Failed to send packet!\n");
        return;
    }

    // Try to receive a response (non-blocking)
    shell_print("Checking for incoming packets...\n");

    uint8_t recv_buf[1518]; // Maximum Ethernet frame
    int count = 0;

    for (int i = 0; i < 5; i++)
    {
        int len = netif_receive(netif, recv_buf, 1518);
        if (len > 0)
        {
            count++;
            shell_print("Received packet: ");
            char num[16];
            int_to_str(len, num);
            shell_print(num);
            shell_print(" bytes\n");

            // Print first 32 bytes in hex
            shell_print("  First 32 bytes:\n  ");
            const char *hex = "0123456789ABCDEF";
            char byte_str[4];
            byte_str[2] = ' ';
            byte_str[3] = '\0';

            for (int j = 0; j < 32 && j < len; j++)
            {
                byte_str[0] = hex[(recv_buf[j] >> 4) & 0xF];
                byte_str[1] = hex[recv_buf[j] & 0xF];
                shell_print(byte_str);

                if ((j + 1) % 16 == 0 && j < 31 && j < len - 1)
                    shell_print("\n  ");
            }
            shell_print("\n");
        }

        // Small delay between checks
        for (volatile int j = 0; j < 1000000; j++)
            ;
    }

    if (count == 0)
    {
        shell_print("No packets received (this is normal if no ARP replies)\n");
    }

    shell_print("\nTest complete!\n");
}

// Command: mkfs - Format a disk
static void cmd_mkfs(const char *args)
{
    if (!args || args[0] == '\0')
    {
        shell_print("\nUsage: mkfs <device>\n");
        shell_print("Example: mkfs hda\n");
        return;
    }

    shell_print("\nFormatting ");
    shell_print(args);
    shell_print(" with VRFS...\n");

    // Get block device
    struct block_device *bdev = blkdev_get(args);
    if (!bdev)
    {
        shell_print("Error: Device not found!\n");
        shell_print("Use 'lsblk' to list available devices.\n");
        return;
    }

    // Format the device
    shell_print("Formatting disk...\n");

    int mkfs_result = vrfs_mkfs(bdev);

    if (mkfs_result != 0)
    {
        shell_print("ERROR: mkfs failed!\n");
        return;
    }

    shell_print("Success! Filesystem created on ");
    shell_print(args);
    shell_print("\n");

    // Verify by reading back directly
    shell_print("Verifying...\n");

    uint8_t *test_buf = (uint8_t *)kmalloc(512);
    if (!test_buf)
    {
        shell_print("ERROR: Cannot allocate test buffer!\n");
        return;
    }

    shell_print("Reading back block 0...\n");
    if (blkdev_read(bdev, 0, test_buf) < 0)
    {
        shell_print("ERROR: Read failed!\n");
        kfree(test_buf);
        return;
    }

    shell_print("Read OK. Checking magic...\n");

    char msg[32];
    const char *hex = "0123456789ABCDEF";

    // Print first 16 bytes (4 x uint32)
    shell_print("First 16 bytes:\n");
    for (int j = 0; j < 4; j++)
    {
        uint32_t val = ((uint32_t *)test_buf)[j];
        msg[0] = ' ';
        msg[1] = ' ';
        msg[2] = '0';
        msg[3] = 'x';
        for (int i = 7; i >= 0; i--)
            msg[4 + (7 - i)] = hex[(val >> (i * 4)) & 0xF];
        msg[12] = '\n';
        msg[13] = '\0';
        shell_print(msg);
    }

    uint32_t *magic = (uint32_t *)test_buf;
    if (*magic == VRFS_MAGIC)
        shell_print("Verification: OK!\n");
    else
        shell_print("Verification: FAILED!\n");

    kfree(test_buf);
}

// Command: mount - Mount a filesystem
static void cmd_mount(const char *args)
{
    // Parse: mount <device> <mount_point>
    if (!args || args[0] == '\0')
    {
        shell_print("\nUsage: mount <device> <mount_point>\n");
        shell_print("Example: mount hda /mnt\n");
        return;
    }

    // Simple parsing: find first space
    const char *device = args;
    const char *mount_point = args;

    while (*mount_point && *mount_point != ' ')
        mount_point++;

    if (*mount_point == '\0')
    {
        shell_print("\nError: Mount point required!\n");
        shell_print("Usage: mount <device> <mount_point>\n");
        return;
    }

    // Skip space
    mount_point++;
    while (*mount_point == ' ')
        mount_point++;

    // Extract device name
    char dev_name[16];
    int i = 0;
    while (device[i] && device[i] != ' ' && i < 15)
    {
        dev_name[i] = device[i];
        i++;
    }
    dev_name[i] = '\0';

    shell_print("\nMounting ");
    shell_print(dev_name);
    shell_print(" at ");
    shell_print(mount_point);
    shell_print("...\n");

    // First test if we can read the superblock
    extern struct block_device *blkdev_get(const char *name);
    struct block_device *test_bdev = blkdev_get(dev_name);
    if (test_bdev)
    {
        shell_print("Device found, testing read...\n");
        extern struct superblock *vrfs_mount(struct block_device * bdev);
        struct superblock *test_sb = vrfs_mount(test_bdev);
        if (test_sb)
        {
            shell_print("Superblock read OK!\n");
            extern int vrfs_unmount(struct superblock * sb);
            vrfs_unmount(test_sb);
        }
        else
        {
            shell_print("ERROR: Cannot read superblock!\n");
            shell_print("The disk may not be formatted or read failed.\n");
            return;
        }
    }

    // Mount using the mount system
    extern int mount_fs(const char *device, const char *path, const char *fstype);

    int result = mount_fs(dev_name, mount_point, "vrfs");
    if (result == -2)
    {
        shell_print("Error: Already mounted! Use 'umount' first.\n");
        return;
    }
    else if (result < 0)
    {
        shell_print("Error: Mount system failed!\n");
        return;
    }

    shell_print("Success! Filesystem mounted at ");
    shell_print(mount_point);
    shell_print("\n");
}

// Command: mount (no args) - Show mounted filesystems
static void cmd_mount_show(void)
{
    shell_print("\nMounted Filesystems:\n");
    shell_print("DEVICE     MOUNT_POINT    TYPE\n");
    shell_print("----------------------------------------\n");

    extern struct mount_point mount_table[MAX_MOUNT_POINTS];

    int count = 0;
    for (int i = 0; i < MAX_MOUNT_POINTS; i++)
    {
        if (mount_table[i].in_use)
        {
            // Print device name
            if (mount_table[i].bdev)
            {
                extern struct block_device *blkdev_get(const char *name);

                // Find device name
                for (int j = 0; j < 4; j++)
                {
                    char dev_name[4];
                    dev_name[0] = 'h';
                    dev_name[1] = 'd';
                    dev_name[2] = 'a' + j;
                    dev_name[3] = '\0';

                    struct block_device *bdev = blkdev_get(dev_name);
                    if (bdev == mount_table[i].bdev)
                    {
                        shell_print(dev_name);
                        shell_print("        ");
                        break;
                    }
                }
            }
            else
            {
                shell_print("none       ");
            }

            // Print mount point
            shell_print(mount_table[i].path);

            // Pad to column
            int len = 0;
            const char *p = mount_table[i].path;
            while (*p++)
                len++;

            for (int j = len; j < 15; j++)
                shell_print(" ");

            // Print filesystem type
            if (mount_table[i].sb && mount_table[i].sb->magic == VRFS_MAGIC)
            {
                shell_print("vrfs\n");
            }
            else
            {
                shell_print("unknown\n");
            }

            count++;
        }
    }

    if (count == 0)
    {
        shell_print("(no mounted filesystems)\n");
    }
    else
    {
        shell_print("\nTotal: ");
        char buffer[16];
        int_to_str(count, buffer);
        shell_print(buffer);
        shell_print(" mounted filesystem(s)\n");
    }
}

// Command: umount - Unmount a filesystem
static void cmd_umount(const char *args)
{
    if (!args || args[0] == '\0')
    {
        shell_print("\nUsage: umount <mount_point>\n");
        shell_print("Example: umount /mnt\n");
        return;
    }

    shell_print("\nUnmounting ");
    shell_print(args);
    shell_print("...\n");

    extern int unmount_fs(const char *path);

    if (unmount_fs(args) < 0)
    {
        shell_print("Error: Unmount failed!\n");
        return;
    }

    shell_print("Success! Filesystem unmounted.\n");
}

// Command: touch - Create an empty file
static void cmd_touch(const char *args)
{
    if (!args || args[0] == '\0')
    {
        shell_print("\nUsage: touch <filename>\n");
        shell_print("Example: touch /mnt/test.txt\n");
        return;
    }

    shell_print("\nCreating file: ");
    shell_print(args);
    shell_print("\n");

    // Get mounted filesystem at /mnt
    extern struct superblock *mount_get_sb(const char *path);
    struct superblock *sb = mount_get_sb("/mnt");

    if (!sb || !sb->root_inode || !sb->root_inode->i_op || !sb->root_inode->i_op->create)
    {
        shell_print("Error: /mnt not mounted or doesn't support file creation!\n");
        return;
    }

    // Normalize the path
    char normalized[256];
    normalize_path(args, normalized);

    // Parse parent directory and filename
    char parent_path[256];
    char filename[64];

    // Find last '/'
    int last_slash = -1;
    for (int i = 0; normalized[i]; i++)
    {
        if (normalized[i] == '/')
            last_slash = i;
    }

    if (last_slash < 0)
    {
        shell_print("Error: Invalid path\n");
        return;
    }

    // Extract parent path
    if (last_slash == 0)
    {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    }
    else
    {
        for (int i = 0; i < last_slash; i++)
            parent_path[i] = normalized[i];
        parent_path[last_slash] = '\0';
    }

    // Extract filename
    int fn_idx = 0;
    for (int i = last_slash + 1; normalized[i] && fn_idx < 63; i++)
        filename[fn_idx++] = normalized[i];
    filename[fn_idx] = '\0';

    if (filename[0] == '\0')
    {
        shell_print("Error: Invalid filename!\n");
        return;
    }

    // Get parent directory inode
    extern struct inode *vfs_lookup_inode(const char *path);
    struct inode *parent = vfs_lookup_inode(parent_path);

    if (!parent || parent->type != VFS_DIRECTORY)
    {
        shell_print("Error: Parent directory not found!\n");
        return;
    }

    if (!parent->i_op || !parent->i_op->create)
    {
        shell_print("Error: Directory doesn't support file creation!\n");
        return;
    }

    // Create the file
    struct inode *new_inode = parent->i_op->create(parent, filename, 0644);
    if (!new_inode)
    {
        shell_print("Error: Failed to create file!\n");
        return;
    }

    shell_print("Success! File created: ");
    shell_print(normalized);
    shell_print("\n");
}

// Command: write - Write text to a file
static void cmd_write(const char *args)
{
    if (!args || args[0] == '\0')
    {
        shell_print("\nUsage: write <filename> <text>\n");
        shell_print("Example: write /mnt/hello.txt Hello World\n");
        return;
    }

    // Parse filename and text
    const char *filename = args;
    const char *text = args;

    while (*text && *text != ' ')
        text++;

    if (*text == '\0')
    {
        shell_print("\nError: Text required!\n");
        return;
    }

    // Skip space
    text++;

    // Get mounted filesystem at /mnt
    extern struct superblock *mount_get_sb(const char *path);
    struct superblock *sb = mount_get_sb("/mnt");

    if (!sb || !sb->root_inode || !sb->root_inode->i_op || !sb->root_inode->i_op->create)
    {
        shell_print("\nError: /mnt not mounted or doesn't support file creation!\n");
        return;
    }

    // Copy filename to temp buffer (stop at space)
    char filepath[256];
    int i;
    for (i = 0; i < 255 && filename[i] && filename[i] != ' '; i++)
        filepath[i] = filename[i];
    filepath[i] = '\0';

    // Normalize the path
    char normalized[256];
    normalize_path(filepath, normalized);

    shell_print("\nWriting to file: ");
    shell_print(normalized);
    shell_print("\n");

    // Parse parent directory and filename
    char parent_path[256];
    char fn[64];

    // Find last '/'
    int last_slash = -1;
    for (int j = 0; normalized[j]; j++)
    {
        if (normalized[j] == '/')
            last_slash = j;
    }

    if (last_slash < 0)
    {
        shell_print("Error: Invalid path\n");
        return;
    }

    // Extract parent path
    if (last_slash == 0)
    {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    }
    else
    {
        for (int j = 0; j < last_slash; j++)
            parent_path[j] = normalized[j];
        parent_path[last_slash] = '\0';
    }

    // Extract filename
    int fn_idx = 0;
    for (int j = last_slash + 1; normalized[j] && fn_idx < 63; j++)
        fn[fn_idx++] = normalized[j];
    fn[fn_idx] = '\0';

    if (fn[0] == '\0')
    {
        shell_print("Error: Invalid filename!\n");
        return;
    }

    // Get parent directory inode
    extern struct inode *vfs_lookup_inode(const char *path);
    struct inode *parent = vfs_lookup_inode(parent_path);

    if (!parent || parent->type != VFS_DIRECTORY)
    {
        shell_print("Error: Parent directory not found!\n");
        return;
    }

    if (!parent->i_op || !parent->i_op->create)
    {
        shell_print("Error: Directory doesn't support file creation!\n");
        return;
    }

    // Create the file
    struct inode *inode = parent->i_op->create(parent, fn, 0644);
    if (!inode)
    {
        shell_print("Error: Failed to create file!\n");
        return;
    }

    // Create a file struct
    struct file file;
    file.inode = inode;
    file.flags = 0;
    file.pos = 0;
    file.f_op = inode->f_op;

    // Calculate text length
    uint32_t text_len = 0;
    const char *t = text;
    while (*t)
    {
        text_len++;
        t++;
    }

    // Write data
    int written = file.f_op->write(&file, text, text_len, 0);
    if (written < 0)
    {
        shell_print("Error: Failed to write data!\n");
        return;
    }

    shell_print("Success! Wrote ");
    char buffer[16];
    int_to_str(written, buffer);
    shell_print(buffer);
    shell_print(" bytes to ");
    shell_print(normalized);
    shell_print("\n");
}

// Command: exectest
static void cmd_exectest(void)
{
    shell_print("\nTesting exec() system call:\n");

    // Check if test program exists
    shell_print("  Checking for /test.bin...\n");

    struct file *f = vfs_open("/test.bin", 0);
    if (!f)
    {
        shell_print("  Error: /test.bin not found!\n");
        shell_print("  Test program was not created.\n");
        shell_print("  Try running 'ls /' to see available files.\n");
        return;
    }
    vfs_close(f);

    shell_print("  Test program found!\n");
    shell_print("\n  exec() System Call Information:\n");
    shell_print("  - Replaces current process with new program\n");
    shell_print("  - Loads code, data, and sets up stack\n");
    shell_print("  - If called from shell, would terminate it!\n");
    shell_print("\n  Typical usage: fork() + exec()\n");
    shell_print("    pid = fork();\n");
    shell_print("    if (pid == 0) {\n");
    shell_print("      execve(\"/test.bin\", argv, envp);\n");
    shell_print("    }\n");

    shell_print("\n  exec() infrastructure is ready!\n");
    shell_print("  File format: Custom EXEC format\n");
    shell_print("  - Magic: 0x45584543 ('EXEC')\n");
    shell_print("  - Sections: text, data, bss, stack\n");
    shell_print("  - User space: 0x08000000-0x0A000000\n");

    shell_print("\nExec test complete!\n");
}

// Execute a command
static void shell_execute_command(void)
{
    // Null-terminate the command
    command_buffer[command_pos] = '\0';

    // Skip empty commands
    if (command_pos == 0)
    {
        shell_print_prompt();
        return;
    }

    // Parse command
    if (strcmp(command_buffer, "help") == 0)
    {
        cmd_help();
    }
    else if (strcmp(command_buffer, "clear") == 0)
    {
        shell_clear_screen();
    }
    else if (strcmp(command_buffer, "cd") == 0)
    {
        cmd_cd(0);
    }
    else if (strncmp(command_buffer, "cd ", 3) == 0)
    {
        cmd_cd(command_buffer + 3);
    }
    else if (strcmp(command_buffer, "about") == 0)
    {
        cmd_about();
    }
    else if (strcmp(command_buffer, "mem") == 0)
    {
        cmd_mem();
    }
    else if (strcmp(command_buffer, "page") == 0)
    {
        cmd_page();
    }
    else if (strcmp(command_buffer, "heap") == 0)
    {
        cmd_heap();
    }
    else if (strcmp(command_buffer, "malloc") == 0)
    {
        cmd_malloc_test();
    }
    else if (strcmp(command_buffer, "ps") == 0)
    {
        cmd_ps();
    }
    else if (strcmp(command_buffer, "syscall") == 0)
    {
        cmd_syscall_test();
    }
    else if (strcmp(command_buffer, "ls") == 0)
    {
        cmd_ls(0);
    }
    else if (strncmp(command_buffer, "ls ", 3) == 0)
    {
        cmd_ls(command_buffer + 3);
    }
    else if (strncmp(command_buffer, "cat ", 4) == 0)
    {
        cmd_cat(command_buffer + 4);
    }
    else if (strcmp(command_buffer, "cat") == 0)
    {
        shell_print("\nUsage: cat <filename>\n");
    }
    else if (strcmp(command_buffer, "rm") == 0)
    {
        shell_print("\nUsage: rm <file>\n");
    }
    else if (strncmp(command_buffer, "rm ", 3) == 0)
    {
        cmd_rm(command_buffer + 3);
    }
    else if (strcmp(command_buffer, "devtest") == 0)
    {
        cmd_devtest();
    }
    else if (strcmp(command_buffer, "usertest") == 0)
    {
        cmd_usertest();
    }
    else if (strcmp(command_buffer, "forktest") == 0)
    {
        cmd_forktest();
    }
    else if (strncmp(command_buffer, "echo ", 5) == 0)
    {
        cmd_echo(command_buffer + 5);
    }
    else if (strcmp(command_buffer, "echo") == 0)
    {
        shell_print("\n");
    }
    else if (strcmp(command_buffer, "exectest") == 0)
    {
        cmd_exectest();
    }
    else if (strncmp(command_buffer, "mkdir ", 6) == 0)
    {
        cmd_mkdir(command_buffer + 6);
    }
    else if (strcmp(command_buffer, "mkdir") == 0)
    {
        shell_print("\nUsage: mkdir <directory>\n");
    }
    else if (strncmp(command_buffer, "rmdir ", 6) == 0)
    {
        cmd_rmdir(command_buffer + 6);
    }
    else if (strcmp(command_buffer, "rmdir") == 0)
    {
        shell_print("\nUsage: rmdir <directory>\n");
    }
    else if (strcmp(command_buffer, "schedtest") == 0)
    {
        cmd_schedtest();
    }
    else if (strcmp(command_buffer, "schedstop") == 0)
    {
        cmd_schedstop();
    }
    else if (strcmp(command_buffer, "ipctest") == 0)
    {
        cmd_ipctest();
    }
    else if (strcmp(command_buffer, "ipcstop") == 0)
    {
        cmd_ipcstop();
    }
    else if (strcmp(command_buffer, "ipcinfo") == 0)
    {
        cmd_ipcinfo();
    }
    else if (strcmp(command_buffer, "drvtest") == 0)
    {
        cmd_drvtest();
    }
    else if (strcmp(command_buffer, "drvstop") == 0)
    {
        cmd_drvstop();
    }
    else if (strcmp(command_buffer, "iotest") == 0)
    {
        cmd_iotest();
    }
    else if (strcmp(command_buffer, "atadrv") == 0)
    {
        cmd_atadrv();
    }
    else if (strcmp(command_buffer, "netdrv") == 0)
    {
        cmd_netdrv();
    }
    else if (strcmp(command_buffer, "blktest") == 0)
    {
        cmd_blktest();
    }
    else if (strcmp(command_buffer, "net2ktest") == 0)
    {
        cmd_net2ktest();
    }
    else if (strcmp(command_buffer, "lsblk") == 0)
    {
        cmd_lsblk();
    }
    else if (strcmp(command_buffer, "atatest") == 0)
    {
        cmd_atatest(0);
    }
    else if (strcmp(command_buffer, "ifconfig") == 0)
    {
        cmd_ifconfig(0);
    }
    else if (strcmp(command_buffer, "nettest") == 0)
    {
        cmd_nettest(0);
    }
    else if (strncmp(command_buffer, "mkfs ", 5) == 0)
    {
        cmd_mkfs(command_buffer + 5);
    }
    else if (strcmp(command_buffer, "mkfs") == 0)
    {
        cmd_mkfs(0);
    }
    else if (strncmp(command_buffer, "mount ", 6) == 0)
    {
        cmd_mount(command_buffer + 6);
    }
    else if (strcmp(command_buffer, "mount") == 0)
    {
        cmd_mount_show();
    }
    else if (strncmp(command_buffer, "umount ", 7) == 0)
    {
        cmd_umount(command_buffer + 7);
    }
    else if (strcmp(command_buffer, "umount") == 0)
    {
        shell_print("\nUsage: umount <mount_point>\n");
    }
    else if (strncmp(command_buffer, "touch ", 6) == 0)
    {
        cmd_touch(command_buffer + 6);
    }
    else if (strcmp(command_buffer, "touch") == 0)
    {
        shell_print("\nUsage: touch <filename>\n");
    }
    else if (strncmp(command_buffer, "write ", 6) == 0)
    {
        cmd_write(command_buffer + 6);
    }
    else if (strcmp(command_buffer, "write") == 0)
    {
        shell_print("\nUsage: write <filename> <text>\n");
    }
    else
    {
        shell_print("\nUnknown command: ");
        shell_print(command_buffer);
        shell_print("\nType 'help' for available commands");
    }

    // Reset command buffer
    command_pos = 0;

    // Print new prompt
    shell_print_prompt();
}

// Initialize shell
void shell_init(void)
{
    command_pos = 0;
    shell_row = 0;
    shell_col = 0;

    shell_clear_screen();
    shell_print("Welcome to VROS Shell!\n");
    shell_print("Type 'help' for available commands.\n");
    shell_print_prompt();
}

// Handle input character
void shell_handle_input(char c)
{
    if (c == '\n')
    {
        // Execute command
        shell_execute_command();
    }
    else if (c == '\b')
    {
        // Backspace
        if (command_pos > 0)
        {
            command_pos--;
            shell_col--;
            if (shell_col < 0)
            {
                shell_col = 79;
                shell_row--;
                if (shell_row < 0)
                {
                    shell_row = 0;
                }
            }
            print_char(' ', shell_col, shell_row);
            update_cursor(shell_col, shell_row);
        }
    }
    else
    {
        // Add character to buffer
        if (command_pos < MAX_COMMAND_LENGTH - 1)
        {
            command_buffer[command_pos++] = c;
            print_char(c, shell_col, shell_row);
            shell_col++;
            if (shell_col >= 80)
            {
                shell_col = 0;
                shell_row++;
                if (shell_row >= 25)
                {
                    shell_row = 24;
                }
            }
            update_cursor(shell_col, shell_row);
        }
    }
}
