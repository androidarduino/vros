#include "shell.h"
#include "pmm.h"
#include "paging.h"
#include "kmalloc.h"
#include "task.h"
#include "syscall.h"
#include "vfs.h"
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
static void cmd_help(void)
{
    shell_print("\nAvailable commands:\n");
    shell_print("  help    - Show this help message\n");
    shell_print("  clear   - Clear the screen\n");
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
    shell_print("  devtest - Test device files\n");
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

    shell_print("\nCurrent Task Information:\n");

    if (current)
    {
        // PID
        shell_print("  PID:    ");
        int_to_str(current->pid, buffer);
        shell_print(buffer);
        shell_print("\n");

        // Name
        shell_print("  Name:   ");
        shell_print(current->name);
        shell_print("\n");

        // State
        shell_print("  State:  ");
        switch (current->state)
        {
        case TASK_RUNNING:
            shell_print("RUNNING");
            break;
        case TASK_READY:
            shell_print("READY");
            break;
        case TASK_BLOCKED:
            shell_print("BLOCKED");
            break;
        case TASK_ZOMBIE:
            shell_print("ZOMBIE");
            break;
        }
        shell_print("\n");
    }
    else
    {
        shell_print("  No current task!\n");
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

// Command: ls helper
static void cmd_ls_dir(const char *path)
{
    shell_print("\nFiles in ");
    shell_print(path);
    shell_print(":\n");

    // Get directory
    struct dentry *dir = vfs_lookup(path);
    if (!dir)
    {
        shell_print("  Error: Cannot access directory\n");
        return;
    }

    // List children
    struct dentry *child = dir->child;
    int count = 0;

    while (child)
    {
        shell_print("  ");

        // Show type indicator
        if (child->inode && child->inode->type == VFS_DIRECTORY)
        {
            shell_print("[DIR]  ");
        }
        else
        {
            shell_print("[FILE] ");
        }

        shell_print(child->name);

        // Show file size for files only
        if (child->inode && child->inode->type != VFS_DIRECTORY)
        {
            shell_print(" (");
            char size_str[16];
            int_to_str(child->inode->size, size_str);
            shell_print(size_str);
            shell_print(" bytes)");
        }

        shell_print("\n");
        child = child->next;
        count++;
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
            // Build path
            char path[256];
            int i = 0;

            // Add leading slash if not present
            if (*args != '/')
            {
                path[0] = '/';
                i = 1;
            }

            while (*args && *args != ' ' && i < 255)
            {
                path[i++] = *args++;
            }
            path[i] = '\0';

            cmd_ls_dir(path);
            return;
        }
    }

    // Default: list root
    cmd_ls_dir("/");
}

// Command: cat
static void cmd_cat(const char *filename)
{
    // Build path
    char path[256];
    int i = 0;

    // Check if path is absolute
    if (filename[0] == '/')
    {
        // Use path as-is
        while (filename[i] && i < 255)
        {
            path[i] = filename[i];
            i++;
        }
        path[i] = '\0';
    }
    else
    {
        // Add leading slash
        path[0] = '/';
        i = 1;
        int j = 0;
        while (filename[j] && i < 255)
        {
            path[i++] = filename[j++];
        }
        path[i] = '\0';
    }

    // Open file
    struct file *f = vfs_open(path, 0);
    if (!f)
    {
        shell_print("\nError: Cannot open file '");
        shell_print(filename);
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
    else if (strcmp(command_buffer, "devtest") == 0)
    {
        cmd_devtest();
    }
    else if (strncmp(command_buffer, "echo ", 5) == 0)
    {
        cmd_echo(command_buffer + 5);
    }
    else if (strcmp(command_buffer, "echo") == 0)
    {
        shell_print("\n");
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
