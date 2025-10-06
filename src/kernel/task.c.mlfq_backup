#include "task.h"
#include "kmalloc.h"
#include "paging.h"
#include "isr.h"

#define MAX_TASKS 32

// Time quantum for each priority level (in timer ticks)
static const uint32_t priority_quantum[PRIORITY_LEVELS] = {
    10, // HIGH: 10 ticks
    5,  // NORMAL: 5 ticks
    3,  // LOW: 3 ticks
    1   // IDLE: 1 tick
};

// Task array and multi-level feedback queues
static struct task *tasks[MAX_TASKS];
static struct task *current_task = 0;
static struct task *ready_queues[PRIORITY_LEVELS] = {0, 0, 0, 0}; // One queue per priority
static uint32_t next_pid = 1;
static int tasking_enabled = 0;
static uint32_t global_ticks = 0; // Global tick counter

// Helper to add task to priority queue
static void queue_add(struct task *task)
{
    task_priority_t prio = task->priority;

    if (!ready_queues[prio])
    {
        ready_queues[prio] = task;
        task->next = task; // Circular queue
    }
    else
    {
        task->next = ready_queues[prio]->next;
        ready_queues[prio]->next = task;
        ready_queues[prio] = task;
    }
}

// Helper to remove task from queue
static void queue_remove(struct task *task)
{
    task_priority_t prio = task->priority;

    if (!ready_queues[prio])
        return;

    // Find task in circular queue
    struct task *current = ready_queues[prio];
    struct task *prev = 0;

    do
    {
        if (current->next == task)
        {
            prev = current;
            break;
        }
        current = current->next;
    } while (current != ready_queues[prio]);

    if (prev)
    {
        prev->next = task->next;
        if (ready_queues[prio] == task)
        {
            if (task->next == task)
                ready_queues[prio] = 0; // Queue is now empty
            else
                ready_queues[prio] = prev;
        }
    }
}

// Get next ready task from highest priority queue
static struct task *get_next_ready_task(void)
{
    // Check each priority level from high to low
    for (int prio = 0; prio < PRIORITY_LEVELS; prio++)
    {
        if (ready_queues[prio])
        {
            // Try to find a ready task in this queue
            struct task *start = ready_queues[prio]->next;
            struct task *current = start;

            do
            {
                if (current->state == TASK_READY || current->state == TASK_RUNNING)
                {
                    ready_queues[prio] = current;
                    return current;
                }
                current = current->next;
            } while (current != start);
        }
    }

    // Fall back to idle task
    return tasks[0];
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
    current_task->priority = PRIORITY_IDLE;
    current_task->base_priority = PRIORITY_IDLE;
    current_task->quantum = priority_quantum[PRIORITY_IDLE];
    current_task->time_slice = current_task->quantum;
    current_task->ticks_used = 0;
    current_task->total_ticks = 0;
    current_task->context_switches = 0;
    current_task->created_time = 0;
    current_task->sleep_until = 0;
    current_task->wait_on = 0;
    current_task->next = 0;
    current_task->parent = 0;
    current_task->child = 0;
    current_task->sibling = 0;
    current_task->exit_code = 0;

    tasks[0] = current_task;
    global_ticks = 0;
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
    new_task->priority = PRIORITY_NORMAL;
    new_task->base_priority = PRIORITY_NORMAL;
    new_task->quantum = priority_quantum[PRIORITY_NORMAL];
    new_task->time_slice = new_task->quantum;
    new_task->ticks_used = 0;
    new_task->total_ticks = 0;
    new_task->context_switches = 0;
    new_task->created_time = global_ticks;
    new_task->sleep_until = 0;
    new_task->wait_on = 0;

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

    // Initialize process tree fields
    new_task->parent = current_task;
    new_task->child = 0;
    new_task->sibling = 0;
    new_task->exit_code = 0;

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
    if (!tasking_enabled)
    {
        return;
    }

    // Increment global tick counter
    global_ticks++;

    // Update current task statistics
    if (current_task && current_task->state == TASK_RUNNING)
    {
        current_task->total_ticks++;
        current_task->ticks_used++;
    }

    // Check for sleeping tasks that need to wake up
    task_check_sleeping();

    // Decrease time slice
    if (current_task->time_slice > 0)
    {
        current_task->time_slice--;
    }

    // If time slice expired or current task is zombie/blocked, switch to next task
    if (current_task->time_slice == 0 ||
        current_task->state == TASK_ZOMBIE ||
        current_task->state == TASK_BLOCKED)
    {
        // Save current task
        struct task *old_task = current_task;

        // Find next ready task (skip zombies and blocked)
        int attempts = 0;
        do
        {
            current_task = get_next_ready_task();

            attempts++;

            // Avoid infinite loop
            if (attempts > MAX_TASKS)
            {
                current_task = tasks[0]; // Fall back to idle task
                break;
            }
        } while (current_task->state == TASK_ZOMBIE ||
                 current_task->state == TASK_BLOCKED);

        // Reset time slice
        current_task->time_slice = current_task->quantum;
        current_task->state = TASK_RUNNING;

        // Add old task back to queue if it's still ready
        if (old_task->state == TASK_RUNNING)
        {
            old_task->state = TASK_READY;
            old_task->time_slice = current_task->quantum;
        }
        // Don't add zombie or blocked tasks back to queue

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

// Find task by PID
struct task *task_find_by_pid(int pid)
{
    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i] && tasks[i]->pid == (uint32_t)pid)
        {
            return tasks[i];
        }
    }
    return 0;
}

// Helper: memory copy
static void memcpy_bytes(void *dest, const void *src, uint32_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (uint32_t i = 0; i < n; i++)
    {
        d[i] = s[i];
    }
}

// Fork current process
int task_fork(void)
{
    if (!current_task)
    {
        return -1;
    }

    // Allocate new task structure
    struct task *child = (struct task *)kmalloc(sizeof(struct task));
    if (!child)
    {
        return -1;
    }

    // Copy everything from parent
    memcpy_bytes(child, current_task, sizeof(struct task));

    // Assign new PID
    child->pid = next_pid++;

    // Allocate new kernel stack
    child->kernel_stack = (uint32_t)kmalloc(4096) + 4096;

    // Copy kernel stack content (approximate)
    // In a real OS, this would be more sophisticated
    child->regs.esp = child->kernel_stack;

    // Initialize process tree fields
    child->parent = current_task;
    child->child = 0;
    child->sibling = current_task->child;
    current_task->child = child;
    child->exit_code = 0;

    // Child returns 0, parent returns child PID
    child->regs.eax = 0; // Child's return value

    // Add to tasks array
    for (int i = 1; i < MAX_TASKS; i++)
    {
        if (!tasks[i])
        {
            tasks[i] = child;
            break;
        }
    }

    // Add child to ready queue
    queue_add(child);

    // Parent returns child's PID
    return child->pid;
}

// Fork current process with register state from syscall
int task_fork_with_regs(struct registers *regs)
{
    if (!current_task || !regs)
    {
        return -1;
    }

    // Allocate new task structure
    struct task *child = (struct task *)kmalloc(sizeof(struct task));
    if (!child)
    {
        return -1;
    }

    // Copy everything from parent
    memcpy_bytes(child, current_task, sizeof(struct task));

    // Assign new PID
    child->pid = next_pid++;

    // Allocate new kernel stack
    child->kernel_stack = (uint32_t)kmalloc(4096) + 4096;
    if (!child->kernel_stack)
    {
        kfree(child);
        return -1;
    }

    // Clone parent's page directory (create independent address space)
    // Note: Only clone if parent has its own directory (not kernel_directory)
    page_directory *parent_dir = (page_directory *)current_task->regs.cr3;
    page_directory *kernel_dir = paging_get_kernel_directory();
    page_directory *child_dir;

    // For now, if parent uses kernel directory, child also uses it
    // This is a simplified implementation - full isolation requires more work
    if (parent_dir == kernel_dir || parent_dir == 0)
    {
        child_dir = kernel_dir;
    }
    else
    {
        // Clone the directory if parent has its own
        child_dir = paging_clone_directory(parent_dir);
        if (!child_dir)
        {
            kfree((void *)(child->kernel_stack - 4096));
            kfree(child);
            return -1;
        }
    }

    // Copy register state from syscall
    // This is the state where fork() was called
    child->regs.eax = 0; // Child returns 0 from fork()
    child->regs.ebx = regs->ebx;
    child->regs.ecx = regs->ecx;
    child->regs.edx = regs->edx;
    child->regs.esi = regs->esi;
    child->regs.edi = regs->edi;
    child->regs.ebp = regs->ebp;
    child->regs.esp = regs->useresp ? regs->useresp : regs->esp;
    child->regs.eip = regs->eip; // Return to same instruction after syscall
    child->regs.eflags = regs->eflags;
    child->regs.cr3 = (uint32_t)child_dir; // Use cloned page directory

    // Initialize process tree fields
    child->parent = current_task;
    child->child = 0;
    child->sibling = current_task->child;
    current_task->child = child;
    child->exit_code = 0;

    // Set child state
    child->state = TASK_READY;
    child->time_slice = current_task->quantum;

    // Add to tasks array
    for (int i = 1; i < MAX_TASKS; i++)
    {
        if (!tasks[i])
        {
            tasks[i] = child;
            break;
        }
    }

    // Add child to ready queue
    queue_add(child);

    // Parent returns child's PID
    return child->pid;
}

// Exit current process
void task_exit(int exit_code)
{
    if (!current_task)
    {
        return;
    }

    // Set exit code and state
    current_task->exit_code = exit_code;
    current_task->state = TASK_ZOMBIE;

    // Force time slice to 0 to trigger immediate reschedule
    current_task->time_slice = 0;

    // Orphan children (reparent to init/idle task)
    struct task *child = current_task->child;
    while (child)
    {
        child->parent = tasks[0]; // Reparent to idle task
        child = child->sibling;
    }

    // Schedule another task immediately
    task_schedule();

    // Should never reach here as we've switched to another task
    while (1)
    {
        __asm__ volatile("hlt");
    }
}

// Wait for child process
int task_waitpid(int pid, int *status)
{
    if (!current_task)
    {
        return -1;
    }

    // Find the child
    struct task *child = task_find_by_pid(pid);
    if (!child || child->parent != current_task)
    {
        return -1; // Not a child of current process
    }

    // Check if child has exited (simplified - no blocking)
    // In a real OS, the parent would be blocked until child exits
    if (child->state != TASK_ZOMBIE)
    {
        return -2; // Child still running (use -2 to distinguish from error)
    }

    // Get exit code
    if (status)
    {
        *status = child->exit_code;
    }

    // Remove child from process tree
    struct task *prev = 0;
    struct task *curr = current_task->child;
    while (curr)
    {
        if (curr == child)
        {
            if (prev)
            {
                prev->sibling = curr->sibling;
            }
            else
            {
                current_task->child = curr->sibling;
            }
            break;
        }
        prev = curr;
        curr = curr->sibling;
    }

    // Remove from tasks array
    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i] == child)
        {
            tasks[i] = 0;
            break;
        }
    }

    // Free resources
    // Free page directory if not using kernel directory
    page_directory *child_dir = (page_directory *)child->regs.cr3;
    page_directory *kernel_dir = paging_get_kernel_directory();

    if (child_dir && child_dir != kernel_dir && child_dir != 0)
    {
        paging_free_directory(child_dir);
    }

    kfree((void *)(child->kernel_stack - 4096));
    kfree(child);

    return pid;
}

// Priority management functions
void task_set_priority(struct task *task, task_priority_t priority)
{
    if (!task || priority >= PRIORITY_LEVELS)
        return;

    // Remove from current queue
    queue_remove(task);

    // Update priority
    task->priority = priority;
    task->base_priority = priority;
    task->quantum = priority_quantum[priority];
    task->ticks_used = 0;

    // Add to new queue if ready
    if (task->state == TASK_READY)
    {
        queue_add(task);
    }
}

task_priority_t task_get_priority(struct task *task)
{
    return task ? task->priority : PRIORITY_IDLE;
}

// Sleep/Wake functions
void task_sleep(uint32_t ticks)
{
    if (!current_task || ticks == 0)
        return;

    current_task->sleep_until = global_ticks + ticks;
    current_task->state = TASK_SLEEPING;
    queue_remove(current_task);

    // Force reschedule
    current_task->time_slice = 0;
    task_schedule();
}

void task_wake(struct task *task)
{
    if (!task || task->state != TASK_SLEEPING)
        return;

    task->sleep_until = 0;
    task->state = TASK_READY;
    task->ticks_used = 0; // Reset for priority boost

    // Priority boost: Reset to base priority when waking from sleep
    if (task->priority > task->base_priority)
    {
        task->priority = task->base_priority;
        task->quantum = priority_quantum[task->priority];
    }

    queue_add(task);
}

void task_check_sleeping(void)
{
    // Check all tasks for wake-up time
    for (int i = 0; i < MAX_TASKS; i++)
    {
        struct task *task = tasks[i];
        if (task &&
            task->state == TASK_SLEEPING &&
            task->sleep_until > 0 &&
            global_ticks >= task->sleep_until)
        {
            task_wake(task);
        }
    }
}

// Statistics functions
uint32_t task_get_ticks(void)
{
    return global_ticks;
}

void task_print_stats(struct task *task)
{
    // This function is meant to be called from shell
    // Stats are: pid, name, state, priority, total_ticks, context_switches
    (void)task; // Defined but implementation in shell
}
