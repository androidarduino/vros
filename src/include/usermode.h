#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

// GDT segment selectors
#define KERNEL_CODE_SEGMENT 0x08
#define KERNEL_DATA_SEGMENT 0x10
#define USER_CODE_SEGMENT 0x18 // Ring 3
#define USER_DATA_SEGMENT 0x20 // Ring 3

// User program structure
struct user_program
{
    void *code;           // Pointer to code
    uint32_t code_size;   // Size of code
    uint32_t entry_point; // Entry point offset
};

// Initialize usermode support
void usermode_init(void);

// Switch to user mode and execute code
void enter_usermode(void *entry_point, void *user_stack);

// Create a user task
int create_user_task(const char *name, void *code, uint32_t code_size);

#endif // USERMODE_H
