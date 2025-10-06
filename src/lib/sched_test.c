#include "task.h"

// Simple test task 1
static void test_task_1(void)
{
    volatile int count = 0;
    while (1)
    {
        count++;
        if (count % 100000 == 0)
        {
            task_yield();
        }
    }
}

// Simple test task 2
static void test_task_2(void)
{
    volatile int count = 0;
    while (1)
    {
        count++;
        if (count % 100000 == 0)
        {
            task_yield();
        }
    }
}

void sched_test_create_tasks(void)
{
    task_create("test1", test_task_1);
    task_create("test2", test_task_2);
}

void sched_test_stop_tasks(void)
{
    // Find and kill test tasks
    for (int i = 0; i < 32; i++)
    {
        struct task *t = task_find_by_pid(i);
        if (t && (t->name[0] == 't' && t->name[1] == 'e' &&
                  t->name[2] == 's' && t->name[3] == 't'))
        {
            t->state = TASK_ZOMBIE;
        }
    }
}
