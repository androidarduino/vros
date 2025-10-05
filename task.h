#ifndef TASK_H
#define TASK_H

#include <stdint.h>

// Process states
typedef enum
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

// CPU registers state (for context switching)
struct registers_state
{
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp;
    uint32_t esp, eip;
    uint32_t eflags;
    uint32_t cr3; // Page directory
} __attribute__((packed));

// Process Control Block (PCB)
struct task
{
    uint32_t pid;                // Process ID
    char name[32];               // Process name
    task_state_t state;          // Process state
    struct registers_state regs; // Saved registers
    uint32_t kernel_stack;       // Kernel stack pointer
    uint32_t time_slice;         // Time slice remaining
    struct task *next;           // Next task in queue
};

// Function declarations
void task_init(void);
uint32_t task_create(const char *name, void (*entry_point)(void));
void task_switch(struct registers_state *old_regs, struct registers_state *new_regs);
void task_schedule(void);
struct task *task_get_current(void);
void task_yield(void);

#endif // TASK_H
