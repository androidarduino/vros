#include "task.h"
#include "kmalloc.h"
#include "paging.h"

#define MAX_TASKS 32
#define TIME_SLICE 5 // 5 timer ticks per task

// Task array and queue
static struct task *tasks[MAX_TASKS];
static struct task *current_task = 0;
static struct task *ready_queue = 0;
static uint32_t next_pid = 1;
static int tasking_enabled = 0;

// Helper to add task to ready queue
static void queue_add(struct task *task)
{
    if (!ready_queue)
    {
        ready_queue = task;
        task->next = task; // Circular queue
    }
    else
    {
        task->next = ready_queue->next;
        ready_queue->next = task;
        ready_queue = task;
    }
}

// Initialize tasking system
void task_init(void)
{
    for (int i = 0; i < MAX_TASKS; i++)
    {
        tasks[i] = 0;
    }

    // Create kernel idle task (current execution)
    current_task = (struct task *)kmalloc(sizeof(struct task));
    current_task->pid = 0;
    for (int i = 0; i < 32; i++)
    {
        current_task->name[i] = 0;
    }
    current_task->name[0] = 'i';
    current_task->name[1] = 'd';
    current_task->name[2] = 'l';
    current_task->name[3] = 'e';
    current_task->state = TASK_RUNNING;
    current_task->time_slice = TIME_SLICE;
    current_task->next = 0;

    tasks[0] = current_task;
    tasking_enabled = 1;
}

// Create a new task
uint32_t task_create(const char *name, void (*entry_point)(void))
{
    // Allocate task structure
    struct task *new_task = (struct task *)kmalloc(sizeof(struct task));
    if (!new_task)
    {
        return 0;
    }

    // Initialize task
    new_task->pid = next_pid++;

    // Copy name
    int i;
    for (i = 0; i < 31 && name[i]; i++)
    {
        new_task->name[i] = name[i];
    }
    new_task->name[i] = '\0';

    new_task->state = TASK_READY;
    new_task->time_slice = TIME_SLICE;

    // Allocate kernel stack (4KB)
    new_task->kernel_stack = (uint32_t)kmalloc(4096) + 4096;

    // Set up initial register state
    new_task->regs.eax = 0;
    new_task->regs.ebx = 0;
    new_task->regs.ecx = 0;
    new_task->regs.edx = 0;
    new_task->regs.esi = 0;
    new_task->regs.edi = 0;
    new_task->regs.ebp = 0;
    new_task->regs.esp = new_task->kernel_stack;
    new_task->regs.eip = (uint32_t)entry_point;
    new_task->regs.eflags = 0x202; // Interrupts enabled
    new_task->regs.cr3 = (uint32_t)paging_get_kernel_directory();

    // Add to tasks array
    for (int i = 1; i < MAX_TASKS; i++)
    {
        if (!tasks[i])
        {
            tasks[i] = new_task;
            break;
        }
    }

    // Add to ready queue
    queue_add(new_task);

    return new_task->pid;
}

// Get current task
struct task *task_get_current(void)
{
    return current_task;
}

// Schedule next task (called from timer interrupt)
void task_schedule(void)
{
    if (!tasking_enabled || !ready_queue)
    {
        return;
    }

    // Decrease time slice
    if (current_task->time_slice > 0)
    {
        current_task->time_slice--;
    }

    // If time slice expired, switch to next task
    if (current_task->time_slice == 0)
    {
        // Save current task
        struct task *old_task = current_task;

        // Get next task from ready queue
        current_task = ready_queue->next;

        // Reset time slice
        current_task->time_slice = TIME_SLICE;
        current_task->state = TASK_RUNNING;

        // Move ready queue pointer
        ready_queue = ready_queue->next;

        // Add old task back to queue if it's still ready
        if (old_task->state == TASK_RUNNING)
        {
            old_task->state = TASK_READY;
            old_task->time_slice = TIME_SLICE;
        }

        // Perform context switch
        if (old_task != current_task)
        {
            task_switch(&old_task->regs, &current_task->regs);
        }
    }
}

// Voluntarily yield CPU
void task_yield(void)
{
    if (!tasking_enabled)
    {
        return;
    }

    current_task->time_slice = 0;
    task_schedule();
}
