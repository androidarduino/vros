#include "usermode.h"
#include "kmalloc.h"
#include "task.h"

// External print function
extern void print_string(const char *str, int row);

// User programs (embedded in kernel for simplicity)
// This is a simple user program that calls a syscall to print a message
// Format: machine code that performs syscall
static unsigned char user_program_hello[] = {
    // mov eax, 1 (SYS_WRITE)
    0xB8, 0x01, 0x00, 0x00, 0x00,
    // mov ebx, 1 (stdout)
    0xBB, 0x01, 0x00, 0x00, 0x00,
    // mov ecx, [message address - will be patched]
    0xB9, 0x00, 0x00, 0x00, 0x00,
    // mov edx, 13 (message length)
    0xBA, 0x0D, 0x00, 0x00, 0x00,
    // int 0x80
    0xCD, 0x80,
    // infinite loop: jmp $
    0xEB, 0xFE};

// Initialize usermode support
void usermode_init(void)
{
    print_string("Usermode support initialized", 27);
}

// Enter user mode (assembly helper will be needed)
// For now, this is a placeholder
void enter_usermode(void *entry_point, void *user_stack)
{
    (void)entry_point;
    (void)user_stack;

    // This needs to be implemented in assembly
    // We'll create a simple version that:
    // 1. Sets up user stack
    // 2. Pushes user data segment
    // 3. Pushes user stack pointer
    // 4. Pushes EFLAGS
    // 5. Pushes user code segment
    // 6. Pushes entry point
    // 7. Uses iret to jump to user mode

    __asm__ volatile(
        "cli\n"
        "mov %0, %%esp\n"   // Set stack pointer
        "mov $0x23, %%ax\n" // User data segment (0x20 | 3 for RPL=3)
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        // Set up stack for iret
        "pushl $0x23\n" // User data segment
        "pushl %0\n"    // User stack pointer
        "pushf\n"       // EFLAGS
        "popl %%eax\n"
        "orl $0x200, %%eax\n" // Enable interrupts in EFLAGS
        "pushl %%eax\n"
        "pushl $0x1B\n" // User code segment (0x18 | 3 for RPL=3)
        "pushl %1\n"    // Entry point
        "iret\n"        // Jump to user mode
        :
        : "r"(user_stack), "r"(entry_point)
        : "eax");
}

// Create a simple test user task
int create_user_task(const char *name, void *code, uint32_t code_size)
{
    (void)name;
    (void)code;
    (void)code_size;

    // Allocate user space (simplified)
    // In a real system, this would set up a new page directory
    void *user_code = kmalloc(4096);  // 4KB for code
    void *user_stack = kmalloc(4096); // 4KB for stack

    if (!user_code || !user_stack)
    {
        return -1;
    }

    // Copy code to user space
    for (uint32_t i = 0; i < code_size && i < 4096; i++)
    {
        ((unsigned char *)user_code)[i] = ((unsigned char *)code)[i];
    }

    // For now, we'll just execute in place
    // In a full implementation, we'd create a new task

    return 0;
}
