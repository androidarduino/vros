#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

// System call numbers
#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_GETPID 3
#define SYS_YIELD 4

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

#endif // SYSCALL_H
