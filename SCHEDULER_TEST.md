# VROS Scheduler Test Guide

## 🎯 Overview

This guide explains how to test the new **Multi-Level Feedback Queue (MLFQ)** scheduler in VROS.

## 📋 What's New

### Multi-Level Feedback Queue Scheduler
- **4 Priority Levels**: HIGH, NORMAL, LOW, IDLE
- **Dynamic Priority Adjustment**: Tasks automatically move between priority levels based on behavior
- **Sleep/Wake Mechanism**: Efficient handling of I/O-bound tasks
- **Process Statistics**: Track CPU time, context switches, and more

## 🧪 Testing the Scheduler

### Step 1: Run VROS
```bash
make run
```

### Step 2: Start Scheduler Test
In the VROS shell, type:
```
schedtest
```

This creates 4 test tasks:
1. **cpu-task** - CPU-bound (will be automatically demoted to lower priority)
2. **io-task** - I/O-bound (sleeps frequently, stays at HIGH priority)
3. **interact** - Interactive (short bursts + sleeps, stays at HIGH priority)
4. **normal** - Mixed workload (NORMAL priority)

### Step 3: Monitor Task Behavior
Use the enhanced `ps` command to see real-time scheduler behavior:
```
ps
```

Output shows:
- **PID**: Process ID
- **Name**: Task name
- **State**: RUNNING, READY, SLEEPING, BLOCKED, or ZOMBIE
- **Priority**: HIGH, NORMAL, LOW, or IDLE
- **CPU(ticks)**: Total CPU time consumed
- **Switches**: Number of context switches

### Step 4: Observe MLFQ in Action

Watch how priorities change over time:

#### Initial State (after `schedtest`):
```
PID  Name         State     Priority  CPU(ticks)  Switches
---  -----------  --------  --------  ----------  --------
0    idle         READY     IDLE      xxx         xx
1    shell        RUNNING   IDLE      xxx         xx
2    cpu-task     READY     NORMAL    0           0
3    io-task      READY     HIGH      0           0
4    interact     READY     HIGH      0           0
5    normal       READY     NORMAL    0           0
```

#### After Running (~10 seconds):
```
PID  Name         State     Priority  CPU(ticks)  Switches
---  -----------  --------  --------  ----------  --------
0    idle         READY     IDLE      50          10
1    shell        RUNNING   IDLE      200         30
2    cpu-task     READY     LOW       500         100  ← Demoted!
3    io-task      SLEEPING  HIGH      100         80   ← Still HIGH
4    interact     SLEEPING  HIGH      150         90   ← Still HIGH
5    normal       READY     NORMAL    250         60
```

**Key Observations:**
- `cpu-task` consumed many ticks → demoted to LOW priority
- `io-task` and `interact` sleep frequently → maintain HIGH priority
- `normal` has mixed behavior → stays at NORMAL priority

### Step 5: Stop the Test
```
schedstop
```

This stops all test tasks and cleans them up.

## 🔬 What to Look For

### 1. **Priority Demotion**
CPU-bound tasks (`cpu-task`) should:
- Start at NORMAL priority
- Gradually be demoted to LOW priority
- Eventually reach IDLE priority (if run long enough)
- Accumulate high tick counts

### 2. **Priority Maintenance**
I/O-bound tasks (`io-task`, `interact`) should:
- Stay at HIGH priority
- Show SLEEPING state frequently
- Have moderate tick counts
- Have high context switch counts

### 3. **Fair Scheduling**
- HIGH priority tasks get more CPU time per quantum (10 ticks)
- NORMAL priority tasks get moderate time (5 ticks)
- LOW priority tasks get less time (3 ticks)
- IDLE tasks get minimal time (1 tick)

### 4. **Sleep/Wake Behavior**
- Sleeping tasks don't consume CPU
- Tasks are automatically woken when sleep expires
- Waking from sleep can boost priority back to base level

## 📊 Understanding the Statistics

### CPU(ticks)
- Total timer ticks while task was running
- Higher = more CPU-intensive
- Compare relative values between tasks

### Switches
- Number of context switches
- Higher = task gives up CPU more frequently
- I/O-bound tasks have high switch counts
- CPU-bound tasks have lower switch counts

### State
- **RUNNING**: Currently executing
- **READY**: In queue, waiting for CPU
- **SLEEPING**: Waiting for timer (not consuming CPU)
- **BLOCKED**: Waiting for resource
- **ZOMBIE**: Exited, waiting for cleanup

## 🎓 Advanced Testing

### Manual Task Creation
You can create tasks programmatically:
```c
uint32_t pid = task_create("my-task", my_function);
```

### Priority Management
```c
struct task *t = task_find_by_pid(pid);
task_set_priority(t, PRIORITY_HIGH);
```

### Sleep API
```c
task_sleep(10); // Sleep for 10 ticks
```

## ✅ Expected Behavior Checklist

- [ ] `schedtest` creates 4 tasks successfully
- [ ] `ps` shows all tasks with statistics
- [ ] CPU-bound task is demoted over time
- [ ] I/O-bound tasks stay at high priority
- [ ] Tasks show SLEEPING state when appropriate
- [ ] Tick counts increase as tasks run
- [ ] Context switch counts are reasonable
- [ ] `schedstop` cleans up test tasks
- [ ] Shell remains responsive throughout

## 🐛 Troubleshooting

### System Freezes
- Check if scheduler is getting called (timer interrupt working)
- Verify `tasking_enabled` is set
- Look for infinite loops in tasks

### Tasks Don't Run
- Check if tasks are in READY state
- Verify they're in the correct priority queue
- Ensure `queue_add()` was called

### Priority Not Changing
- Check `ticks_used` vs `quantum`
- Verify demotion logic in `task_schedule()`
- Look for tasks that yield too frequently

## 📚 Related Files

- `src/include/task.h` - Task structure and API
- `src/kernel/task.c` - Scheduler implementation
- `src/lib/sched_test.c` - Test task implementations
- `src/lib/shell.c` - Shell commands (ps, schedtest, schedstop)

## 🎉 Success Criteria

Your scheduler is working correctly if:
1. ✅ Tasks execute concurrently
2. ✅ Priority levels are enforced (HIGH tasks run more)
3. ✅ CPU-bound tasks are automatically demoted
4. ✅ I/O-bound tasks maintain high priority
5. ✅ Sleep/wake mechanism works correctly
6. ✅ Statistics are tracked accurately
7. ✅ System remains stable under load

---

**Congratulations!** You now have a modern, production-quality task scheduler! 🚀

