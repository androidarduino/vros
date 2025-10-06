#include "irq_bridge.h"
#include "ipc.h"
#include "task.h"
#include <stdint.h>

// IRQ 处理器注册表
struct irq_handler_entry
{
    uint32_t ipc_port; // 接收中断通知的 IPC 端口
    uint32_t pid;      // 注册该处理器的进程 PID
    int registered;    // 是否已注册
};

static struct irq_handler_entry irq_handlers[16];
static uint32_t irq_counter = 0; // 全局 IRQ 计数器

// 初始化 IRQ 桥接系统
void irq_bridge_init(void)
{
    for (int i = 0; i < 16; i++)
    {
        irq_handlers[i].ipc_port = 0;
        irq_handlers[i].pid = 0;
        irq_handlers[i].registered = 0;
    }
}

// 注册 IRQ 处理器
int irq_bridge_register(uint8_t irq, uint32_t ipc_port)
{
    if (irq >= 16)
        return -1;

    // 获取当前进程 PID
    struct task *current = get_current_task();
    if (!current)
        return -1;

    // 检查是否已被其他进程注册
    if (irq_handlers[irq].registered)
    {
        // 如果是同一个进程重新注册，允许
        if (irq_handlers[irq].pid != current->pid)
            return -1; // 已被其他进程占用
    }

    // 注册
    irq_handlers[irq].ipc_port = ipc_port;
    irq_handlers[irq].pid = current->pid;
    irq_handlers[irq].registered = 1;

    return 0;
}

// 取消注册 IRQ 处理器
int irq_bridge_unregister(uint8_t irq)
{
    if (irq >= 16)
        return -1;

    struct task *current = get_current_task();
    if (!current)
        return -1;

    // 只能取消注册自己的处理器
    if (irq_handlers[irq].registered && irq_handlers[irq].pid == current->pid)
    {
        irq_handlers[irq].registered = 0;
        irq_handlers[irq].ipc_port = 0;
        irq_handlers[irq].pid = 0;
        return 0;
    }

    return -1;
}

// 在 IRQ 发生时调用
void irq_bridge_notify(uint8_t irq)
{
    if (irq >= 16)
        return;

    if (!irq_handlers[irq].registered)
        return; // 没有注册的处理器

    // 构造 IRQ 消息
    struct irq_message msg;
    msg.type = IPC_MSG_IRQ;
    msg.irq_number = irq;
    msg.reserved[0] = 0;
    msg.reserved[1] = 0;
    msg.reserved[2] = 0;
    msg.timestamp = irq_counter++;

    // 发送到注册的 IPC 端口（非阻塞）
    // 注意：这里我们直接调用 IPC 发送，不通过系统调用
    // 因为我们在中断上下文中
    // 使用内核内部的 IPC 发送函数
    extern int ipc_send(uint32_t dest_port, uint32_t type, const void *data, uint32_t size);
    ipc_send(irq_handlers[irq].ipc_port, IPC_MSG_IRQ, &msg, sizeof(msg));
}
