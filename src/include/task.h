#ifndef TASK_H
#define TASK_H

#include <stdint.h>

// Process states
typedef enum
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_ZOMBIE
} task_state_t;

// Process priority levels
typedef enum
{
    PRIORITY_HIGH = 0,
    PRIORITY_NORMAL = 1,
    PRIORITY_LOW = 2,
    PRIORITY_IDLE = 3,
    PRIORITY_LEVELS = 4
} task_priority_t;

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

    // Priority and scheduling
    task_priority_t priority;      // Current priority level
    task_priority_t base_priority; // Base priority (for restoration)
    uint32_t quantum;              // Time quantum for this priority
    uint32_t ticks_used;           // Ticks used in current quantum

    // Statistics
    uint32_t total_ticks;      // Total CPU time used
    uint32_t context_switches; // Number of context switches
    uint32_t created_time;     // When task was created (in ticks)

    // Sleep/Wake
    uint32_t sleep_until; // Wake up time (0 = not sleeping)
    struct task *wait_on; // Task we're waiting for

    // Process tree
    struct task *parent;  // Parent process
    struct task *child;   // First child process
    struct task *sibling; // Next sibling process
    int exit_code;        // Exit code (for zombies)

    // I/O Permission Bitmap (for user-space drivers)
    uint8_t *iopb; // I/O Permission Bitmap (8192 bytes)
};

// Function declarations
void task_init(void);
uint32_t task_create(const char *name, void (*entry_point)(void));
void task_switch(struct registers_state *old_regs, struct registers_state *new_regs);
void task_schedule(void);
struct task *task_get_current(void);
struct task *get_current_task(void); // Alias for task_get_current
void task_yield(void);

// Priority management
void task_set_priority(struct task *task, task_priority_t priority);
task_priority_t task_get_priority(struct task *task);

// Sleep/Wake
void task_sleep(uint32_t ticks);
void task_wake(struct task *task);
void task_check_sleeping(void); // Called by timer

// Forward declaration for registers
struct registers;

// Fork and process management
int task_fork(void);
int task_fork_with_regs(struct registers *regs);
void task_exit(int exit_code);
int task_waitpid(int pid, int *status);
struct task *task_find_by_pid(int pid);

// Statistics
uint32_t task_get_ticks(void);
void task_print_stats(struct task *task);

#endif // TASK_H
