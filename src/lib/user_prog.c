// Simple user program examples
// These will be compiled and embedded in the kernel

#include <stdint.h>

// Helper function to make a system call
static inline int syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1));
    return ret;
}

// User program: print message and exit
void user_program_test(void)
{
    // Use syscall to get PID
    int pid = syscall1(3, 0); // SYS_GETPID

    // Infinite loop (will be killed by scheduler)
    while (1)
    {
        // Yield CPU
        syscall1(4, 0); // SYS_YIELD
    }
}

// Get the start address of user program
void *get_user_program_test(void)
{
    return (void *)user_program_test;
}

// Calculate size (approximate)
uint32_t get_user_program_test_size(void)
{
    return 100; // Approximate size
}
