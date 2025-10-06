#ifndef IRQ_BRIDGE_H
#define IRQ_BRIDGE_H

#include <stdint.h>

// IRQ 到 IPC 桥接
// 允许用户空间进程通过 IPC 接收硬件中断通知

// IRQ 消息类型
#define IPC_MSG_IRQ 0x1000

// IRQ 消息结构
struct irq_message
{
    uint32_t type;      // IPC_MSG_IRQ
    uint8_t irq_number; // IRQ 号 (0-15)
    uint8_t reserved[3];
    uint32_t timestamp; // 中断发生的时间戳
};

// 初始化 IRQ 桥接系统
void irq_bridge_init(void);

// 注册 IRQ 处理器
// irq: IRQ 号 (0-15)
// ipc_port: 接收中断通知的 IPC 端口
// 返回: 0 成功, -1 失败
int irq_bridge_register(uint8_t irq, uint32_t ipc_port);

// 取消注册 IRQ 处理器
int irq_bridge_unregister(uint8_t irq);

// 在 IRQ 发生时调用（从内核 IRQ 处理器调用）
void irq_bridge_notify(uint8_t irq);

#endif // IRQ_BRIDGE_H
