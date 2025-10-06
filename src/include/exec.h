#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

// Simple executable format header
// This is a simplified format for our microkernel
struct exec_header
{
    uint32_t magic;      // Magic number: 0x45584543 ("EXEC")
    uint32_t entry;      // Entry point address
    uint32_t text_size;  // Size of code section
    uint32_t data_size;  // Size of data section
    uint32_t bss_size;   // Size of BSS section
    uint32_t stack_size; // Stack size
};

// Magic number for our executable format
#define EXEC_MAGIC 0x45584543

// Default user space addresses
#define USER_TEXT_START 0x08000000 // 128MB - where user code loads
#define USER_DATA_START 0x08100000 // 129MB - where user data loads
#define USER_STACK_TOP 0x0A000000  // 160MB - top of user stack

// Function declarations
int exec_load(const char *path, const char **argv);
int sys_execve(const char *path, const char **argv, const char **envp);

#endif // EXEC_H
