#include "syscall.h"
#include "idt.h"
#include "task.h"
#include "isr.h"
#include "exec.h"
#include "ipc.h"
#include "ioport.h"
#include "irq_bridge.h"

// External assembly syscall handler
extern void syscall_asm_handler(void);

// System call table
static syscall_handler_t syscall_table[SYSCALL_MAX];

// External print functions
extern void print_string(const char *str, int row);
extern void print_char(char c, int col, int row);

// Syscall: exit
int sys_exit(int status)
{
    (void)status; // Unused for now

    // For now, just halt
    while (1)
    {
        __asm__ volatile("hlt");
    }

    return 0;
}

// Syscall: write (simplified - only writes to screen)
int sys_write(int fd, const char *buf, int count)
{
    (void)fd; // Ignore fd for now, always write to screen

    // Use shell's scrolling output mechanism
    extern void shell_print_raw(const char *str, int len);
    shell_print_raw(buf, count);

    return count;
}

// Syscall: read (not implemented yet)
int sys_read(int fd, char *buf, int count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

// Syscall: getpid
int sys_getpid(void)
{
    struct task *current = task_get_current();
    if (current)
    {
        return current->pid;
    }
    return -1;
}

// Syscall: yield
int sys_yield(void)
{
    task_yield();
    return 0;
}

// Syscall: fork (needs register state)
int sys_fork(struct registers *regs)
{
    return task_fork_with_regs(regs);
}

// Syscall: waitpid
int sys_waitpid(int pid, int *status)
{
    return task_waitpid(pid, status);
}

// Syscall: IPC create port
int sys_ipc_create_port(void)
{
    return ipc_create_port();
}

// Syscall: IPC create named port
int sys_ipc_create_named_port(const char *name)
{
    return ipc_create_named_port(name);
}

// Syscall: IPC destroy port
int sys_ipc_destroy_port(uint32_t port_id)
{
    return ipc_destroy_port(port_id);
}

// Syscall: IPC send
int sys_ipc_send(uint32_t dest_port, uint32_t type, const void *data, uint32_t size)
{
    return ipc_send(dest_port, type, data, size);
}

// Syscall: IPC recv
int sys_ipc_recv(uint32_t port_id, void *msg)
{
    return ipc_recv(port_id, (struct ipc_message *)msg);
}

// Syscall: IPC try recv (non-blocking)
int sys_ipc_try_recv(uint32_t port_id, void *msg)
{
    return ipc_try_recv(port_id, (struct ipc_message *)msg);
}

// Syscall: IPC find port
int sys_ipc_find_port(const char *name)
{
    return ipc_find_port(name);
}

// System call handler (called from ISR)
void syscall_handler(struct registers *regs)
{
    // Get syscall number from EAX
    uint32_t syscall_num = regs->eax;

    // Check if valid
    if (syscall_num >= SYSCALL_MAX || !syscall_table[syscall_num])
    {
        regs->eax = -1; // Return error
        return;
    }

    // Special handling for fork (needs register state)
    if (syscall_num == SYS_FORK)
    {
        int ret = sys_fork(regs);
        regs->eax = ret;
        return;
    }

    // Call the appropriate handler
    syscall_handler_t handler = syscall_table[syscall_num];
    int ret = handler(regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);

    // Return value in EAX
    regs->eax = ret;
}

// Initialize system calls
void syscall_init(void)
{
    // Clear syscall table
    for (int i = 0; i < SYSCALL_MAX; i++)
    {
        syscall_table[i] = 0;
    }

    // Register system calls
    syscall_table[SYS_EXIT] = (syscall_handler_t)sys_exit;
    syscall_table[SYS_WRITE] = (syscall_handler_t)sys_write;
    syscall_table[SYS_READ] = (syscall_handler_t)sys_read;
    syscall_table[SYS_GETPID] = (syscall_handler_t)sys_getpid;
    syscall_table[SYS_YIELD] = (syscall_handler_t)sys_yield;
    syscall_table[SYS_FORK] = (syscall_handler_t)sys_fork;
    syscall_table[SYS_WAITPID] = (syscall_handler_t)sys_waitpid;
    syscall_table[SYS_EXECVE] = (syscall_handler_t)sys_execve;
    syscall_table[SYS_IPC_CREATE_PORT] = (syscall_handler_t)sys_ipc_create_port;
    syscall_table[SYS_IPC_CREATE_NAMED_PORT] = (syscall_handler_t)sys_ipc_create_named_port;
    syscall_table[SYS_IPC_DESTROY_PORT] = (syscall_handler_t)sys_ipc_destroy_port;
    syscall_table[SYS_IPC_SEND] = (syscall_handler_t)sys_ipc_send;
    syscall_table[SYS_IPC_RECV] = (syscall_handler_t)sys_ipc_recv;
    syscall_table[SYS_IPC_TRY_RECV] = (syscall_handler_t)sys_ipc_try_recv;
    syscall_table[SYS_IPC_FIND_PORT] = (syscall_handler_t)sys_ipc_find_port;
    syscall_table[SYS_REQUEST_IO_PORT] = (syscall_handler_t)sys_request_io_port;
    syscall_table[SYS_REGISTER_IRQ_HANDLER] = (syscall_handler_t)sys_register_irq_handler;

    // Register INT 0x80 in IDT (0xEE = present, ring 3, 32-bit trap gate)
    idt_set_gate(0x80, (uint32_t)syscall_asm_handler, 0x08, 0xEE);
}

// Syscall: request_io_port - 请求 I/O 端口访问权限
int sys_request_io_port(uint16_t port_start, uint16_t port_end)
{
    // 只有特权进程才能请求 I/O 端口访问
    // 这里简单起见，允许所有用户进程请求
    // 在生产系统中，应该有更严格的权限检查

    return ioport_grant_access(port_start, port_end);
}

// Syscall: register_irq_handler - 注册 IRQ 处理器
int sys_register_irq_handler(uint8_t irq, uint32_t ipc_port)
{
    return irq_bridge_register(irq, ipc_port);
}
