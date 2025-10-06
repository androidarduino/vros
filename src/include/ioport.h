#ifndef IOPORT_H
#define IOPORT_H

#include <stdint.h>

// I/O Permission Bitmap 管理
// 允许用户空间进程访问特定的 I/O 端口

// 初始化 I/O 权限系统
void ioport_init(void);

// 为当前任务授予 I/O 端口访问权限
// port_start: 起始端口
// port_end: 结束端口（包含）
// 返回: 0 成功, -1 失败
int ioport_grant_access(uint16_t port_start, uint16_t port_end);

// 撤销当前任务的 I/O 端口访问权限
int ioport_revoke_access(uint16_t port_start, uint16_t port_end);

// 检查当前任务是否有权限访问指定端口
int ioport_check_access(uint16_t port);

#endif // IOPORT_H
