#include "syscall.h"
#include "idt.h"
#include "task.h"
#include "isr.h"

// External assembly syscall handler
extern void syscall_asm_handler(void);

// System call table
static syscall_handler_t syscall_table[SYSCALL_MAX];

// External print functions
extern void print_string(const char *str, int row);
extern void print_char(char c, int col, int row);

// Syscall: exit
int sys_exit(int status)
{
    (void)status; // Unused for now

    // For now, just halt
    while (1)
    {
        __asm__ volatile("hlt");
    }

    return 0;
}

// Syscall: write (simplified - only writes to screen)
int sys_write(int fd, const char *buf, int count)
{
    (void)fd; // Ignore fd for now, always write to screen

    // Find a free row to write (simple implementation)
    static int write_row = 20;

    for (int i = 0; i < count && buf[i] != '\0'; i++)
    {
        print_char(buf[i], i, write_row);
    }

    return count;
}

// Syscall: read (not implemented yet)
int sys_read(int fd, char *buf, int count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

// Syscall: getpid
int sys_getpid(void)
{
    struct task *current = task_get_current();
    if (current)
    {
        return current->pid;
    }
    return -1;
}

// Syscall: yield
int sys_yield(void)
{
    task_yield();
    return 0;
}

// System call handler (called from ISR)
void syscall_handler(struct registers *regs)
{
    // Get syscall number from EAX
    uint32_t syscall_num = regs->eax;

    // Check if valid
    if (syscall_num >= SYSCALL_MAX || !syscall_table[syscall_num])
    {
        regs->eax = -1; // Return error
        return;
    }

    // Call the appropriate handler
    syscall_handler_t handler = syscall_table[syscall_num];
    int ret = handler(regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);

    // Return value in EAX
    regs->eax = ret;
}

// Initialize system calls
void syscall_init(void)
{
    // Clear syscall table
    for (int i = 0; i < SYSCALL_MAX; i++)
    {
        syscall_table[i] = 0;
    }

    // Register system calls
    syscall_table[SYS_EXIT] = (syscall_handler_t)sys_exit;
    syscall_table[SYS_WRITE] = (syscall_handler_t)sys_write;
    syscall_table[SYS_READ] = (syscall_handler_t)sys_read;
    syscall_table[SYS_GETPID] = (syscall_handler_t)sys_getpid;
    syscall_table[SYS_YIELD] = (syscall_handler_t)sys_yield;

    // Register INT 0x80 in IDT (0xEE = present, ring 3, 32-bit trap gate)
    idt_set_gate(0x80, (uint32_t)syscall_asm_handler, 0x08, 0xEE);
}
