#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

// System call numbers
#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_GETPID 3
#define SYS_YIELD 4
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_EXECVE 7
#define SYS_IPC_CREATE_PORT 8
#define SYS_IPC_DESTROY_PORT 9
#define SYS_IPC_SEND 10
#define SYS_IPC_RECV 11
#define SYS_IPC_CREATE_NAMED_PORT 12
#define SYS_IPC_FIND_PORT 13
#define SYS_IPC_TRY_RECV 14

// Maximum number of system calls
#define SYSCALL_MAX 256

// System call handler type
typedef int (*syscall_handler_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

// Forward declaration
struct registers;

// Function declarations
void syscall_init(void);
void syscall_handler(struct registers *regs);
int sys_exit(int status);
int sys_write(int fd, const char *buf, int count);
int sys_read(int fd, char *buf, int count);
int sys_getpid(void);
int sys_yield(void);
int sys_fork(struct registers *regs);
int sys_waitpid(int pid, int *status);
int sys_execve(const char *path, const char **argv, const char **envp);
int sys_ipc_create_port(void);
int sys_ipc_create_named_port(const char *name);
int sys_ipc_destroy_port(uint32_t port_id);
int sys_ipc_send(uint32_t dest_port, uint32_t type, const void *data, uint32_t size);
int sys_ipc_recv(uint32_t port_id, void *msg);
int sys_ipc_try_recv(uint32_t port_id, void *msg);
int sys_ipc_find_port(const char *name);

#endif // SYSCALL_H
