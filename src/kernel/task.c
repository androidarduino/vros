#include "task.h"
#include "kmalloc.h"
#include "paging.h"
#include "isr.h"

#define MAX_TASKS 32
#define TIME_SLICE 5

// Simple single queue
static struct task *tasks[MAX_TASKS];
static struct task *current_task = 0;
static struct task *ready_queue = 0; // Simple circular queue
static uint32_t next_pid = 1;
static int tasking_enabled = 0;
static uint32_t global_ticks = 0;

// Simple queue operations
static void simple_queue_add(struct task *task)
{
    if (!ready_queue)
    {
        ready_queue = task;
        task->next = task;
    }
    else
    {
        task->next = ready_queue->next;
        ready_queue->next = task;
        ready_queue = task;
    }
}

// Initialize tasking
void task_init(void)
{
    for (int i = 0; i < MAX_TASKS; i++)
        tasks[i] = 0;

    current_task = (struct task *)kmalloc(sizeof(struct task));
    current_task->pid = 0;

    // Set name
    current_task->name[0] = 'i';
    current_task->name[1] = 'd';
    current_task->name[2] = 'l';
    current_task->name[3] = 'e';
    current_task->name[4] = '\0';

    current_task->state = TASK_RUNNING;
    current_task->time_slice = TIME_SLICE;
    current_task->priority = PRIORITY_IDLE;
    current_task->base_priority = PRIORITY_IDLE;
    current_task->quantum = TIME_SLICE;
    current_task->ticks_used = 0;
    current_task->total_ticks = 0;
    current_task->context_switches = 0;
    current_task->created_time = 0;
    current_task->sleep_until = 0;
    current_task->wait_on = 0;
    current_task->parent = 0;
    current_task->child = 0;
    current_task->sibling = 0;
    current_task->exit_code = 0;
    current_task->iopb = NULL; // Initialize I/O permission bitmap

    // IMPORTANT: Idle task must be in the queue!
    current_task->next = current_task; // Points to itself
    ready_queue = current_task;        // Queue contains only idle

    tasks[0] = current_task;
    tasking_enabled = 1;
}

// Create a task
uint32_t task_create(const char *name, void (*entry_point)(void))
{
    // Disable interrupts during task creation to prevent scheduling
    __asm__ volatile("cli");

    struct task *new_task = (struct task *)kmalloc(sizeof(struct task));
    if (!new_task)
    {
        __asm__ volatile("sti");
        return 0;
    }

    new_task->pid = next_pid++;

    // Copy name
    int i;
    for (i = 0; i < 31 && name[i]; i++)
        new_task->name[i] = name[i];
    new_task->name[i] = '\0';

    new_task->state = TASK_READY;
    new_task->time_slice = TIME_SLICE;
    new_task->priority = PRIORITY_NORMAL;
    new_task->base_priority = PRIORITY_NORMAL;
    new_task->quantum = TIME_SLICE;
    new_task->ticks_used = 0;
    new_task->total_ticks = 0;
    new_task->context_switches = 0;
    new_task->created_time = global_ticks;
    new_task->sleep_until = 0;
    new_task->wait_on = 0;

    new_task->kernel_stack = (uint32_t)kmalloc(4096) + 4096;

    new_task->regs.eax = 0;
    new_task->regs.ebx = 0;
    new_task->regs.ecx = 0;
    new_task->regs.edx = 0;
    new_task->regs.esi = 0;
    new_task->regs.edi = 0;
    new_task->regs.ebp = 0;
    new_task->regs.esp = new_task->kernel_stack;
    new_task->regs.eip = (uint32_t)entry_point;
    new_task->regs.eflags = 0x202; // NOTE: Interrupts enabled in new task
    new_task->regs.cr3 = (uint32_t)paging_get_kernel_directory();

    new_task->parent = current_task;
    new_task->child = 0;
    new_task->sibling = 0;
    new_task->exit_code = 0;
    new_task->iopb = NULL; // Initialize I/O permission bitmap

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
    simple_queue_add(new_task);

    // Re-enable interrupts
    __asm__ volatile("sti");

    return new_task->pid;
}

struct task *task_get_current(void)
{
    return current_task;
}

// Alias for compatibility
struct task *get_current_task(void)
{
    return task_get_current();
}

// Simplified scheduler
void task_schedule(void)
{
    if (!tasking_enabled || !ready_queue)
        return;

    global_ticks++;

    if (current_task && current_task->state == TASK_RUNNING)
    {
        current_task->total_ticks++;
    }

    if (current_task->time_slice > 0)
    {
        current_task->time_slice--;
    }

    if (current_task->time_slice == 0 ||
        current_task->state == TASK_ZOMBIE ||
        current_task->state == TASK_BLOCKED)
    {
        struct task *old_task = current_task;

        // Find next non-zombie task
        struct task *next = ready_queue->next;
        int attempts = 0;
        int max_tasks = 32;

        // Skip zombie tasks
        while (next->state == TASK_ZOMBIE && attempts < max_tasks)
        {
            ready_queue = ready_queue->next;
            next = ready_queue->next;
            attempts++;
        }

        current_task = next;
        ready_queue = next;

        // Only set to RUNNING if not already ZOMBIE
        if (current_task->state != TASK_ZOMBIE)
        {
            current_task->time_slice = TIME_SLICE;
            current_task->state = TASK_RUNNING;
            current_task->context_switches++;
        }

        if (old_task->state == TASK_RUNNING)
        {
            old_task->state = TASK_READY;
        }

        if (old_task != current_task)
        {
            task_switch(&old_task->regs, &current_task->regs);
        }
    }
}

void task_yield(void)
{
    if (!tasking_enabled)
        return;
    current_task->time_slice = 0;
    task_schedule();
}

struct task *task_find_by_pid(int pid)
{
    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i] && tasks[i]->pid == (uint32_t)pid)
            return tasks[i];
    }
    return 0;
}

// Stub implementations for compatibility
void task_set_priority(struct task *task, task_priority_t priority)
{
    if (task)
    {
        task->priority = priority;
        task->base_priority = priority;
    }
}

task_priority_t task_get_priority(struct task *task)
{
    return task ? task->priority : PRIORITY_IDLE;
}

void task_sleep(uint32_t ticks) { (void)ticks; }
void task_wake(struct task *task) { (void)task; }
void task_check_sleeping(void) {}
uint32_t task_get_ticks(void) { return global_ticks; }
void task_print_stats(struct task *task) { (void)task; }

// Keep fork/exit/waitpid stubs for now
int task_fork_with_regs(struct registers *regs)
{
    (void)regs;
    return -1;
}
void task_exit(int exit_code)
{
    (void)exit_code;
    current_task->state = TASK_ZOMBIE;
    while (1)
        ;
}
int task_waitpid(int pid, int *status)
{
    (void)pid;
    (void)status;
    return -1;
}
