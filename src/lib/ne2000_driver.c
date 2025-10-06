/*
 * 用户空间 NE2000 网络驱动
 *
 * 通过 IPC 与内核通信
 */

#include <stdint.h>
#include "netdev_ipc.h"

// NE2000 寄存器定义
#define NE2000_BASE 0x300

#define NE2000_CMD (NE2000_BASE + 0x00)
#define NE2000_CLDA0 (NE2000_BASE + 0x01)
#define NE2000_CLDA1 (NE2000_BASE + 0x02)
#define NE2000_BNRY (NE2000_BASE + 0x03)
#define NE2000_TSR (NE2000_BASE + 0x04)
#define NE2000_NCR (NE2000_BASE + 0x05)
#define NE2000_FIFO (NE2000_BASE + 0x06)
#define NE2000_ISR (NE2000_BASE + 0x07)
#define NE2000_CRDA0 (NE2000_BASE + 0x08)
#define NE2000_CRDA1 (NE2000_BASE + 0x09)
#define NE2000_RSR (NE2000_BASE + 0x0C)
#define NE2000_CNTR0 (NE2000_BASE + 0x0D)
#define NE2000_CNTR1 (NE2000_BASE + 0x0E)
#define NE2000_CNTR2 (NE2000_BASE + 0x0F)
#define NE2000_DATAPORT (NE2000_BASE + 0x10)
#define NE2000_RESET (NE2000_BASE + 0x1F)

// Page 0 registers
#define NE2000_PSTART (NE2000_BASE + 0x01)
#define NE2000_PSTOP (NE2000_BASE + 0x02)
#define NE2000_TPSR (NE2000_BASE + 0x04)
#define NE2000_TBCR0 (NE2000_BASE + 0x05)
#define NE2000_TBCR1 (NE2000_BASE + 0x06)
#define NE2000_RSAR0 (NE2000_BASE + 0x08)
#define NE2000_RSAR1 (NE2000_BASE + 0x09)
#define NE2000_RBCR0 (NE2000_BASE + 0x0A)
#define NE2000_RBCR1 (NE2000_BASE + 0x0B)
#define NE2000_RCR (NE2000_BASE + 0x0C)
#define NE2000_TCR (NE2000_BASE + 0x0D)
#define NE2000_DCR (NE2000_BASE + 0x0E)
#define NE2000_IMR (NE2000_BASE + 0x0F)

// Page 1 registers
#define NE2000_PAR0 (NE2000_BASE + 0x01)
#define NE2000_CURR (NE2000_BASE + 0x07)

// 命令寄存器位
#define NE2000_CMD_PAGE0 0x00
#define NE2000_CMD_PAGE1 0x40
#define NE2000_CMD_STOP 0x01
#define NE2000_CMD_START 0x02
#define NE2000_CMD_TRANS 0x04
#define NE2000_CMD_RREAD 0x08
#define NE2000_CMD_RWRITE 0x10
#define NE2000_CMD_NODMA 0x20

// 系统调用号
#define SYS_WRITE 1
#define SYS_IPC_CREATE_NAMED_PORT 11
#define SYS_IPC_SEND 12
#define SYS_IPC_RECV 13
#define SYS_REQUEST_IO_PORT 15
#define SYS_REGISTER_IRQ_HANDLER 16

// IPC 消息结构（与内核匹配）
struct ipc_message_user
{
    uint32_t sender_pid;
    uint32_t sender_port;
    uint32_t type;
    uint32_t size;
    char data[256];
};

// 系统调用封装
static inline int syscall_request_io_port(uint16_t port_start, uint16_t port_end)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_REQUEST_IO_PORT), "b"(port_start), "c"(port_end));
    return ret;
}

static inline int syscall_ipc_create_named_port(const char *name)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_CREATE_NAMED_PORT), "b"(name));
    return ret;
}

static inline int syscall_ipc_recv(uint32_t port, struct ipc_message_user *msg)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_RECV), "b"(port), "c"(msg));
    return ret;
}

static inline int syscall_ipc_send(uint32_t src_port, uint32_t dst_port, const void *msg, uint32_t size)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_SEND), "b"(src_port), "c"(dst_port), "d"(msg), "S"(size));
    return ret;
}

// I/O 端口操作
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// 全局变量
static uint8_t mac_addr[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint8_t rx_page_start = 0x46;
static uint8_t rx_page_stop = 0x80;
static uint8_t next_packet = 0x46;

// 简单的 memcpy
static void *memcpy(void *dest, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

// 初始化 NE2000（简化版，仅读取 MAC 地址）
static int ne2000_init(void)
{
    // 简单读取 MAC 地址，不进行复杂的初始化
    // 避免在驱动初始化时卡住

    // 如果设备不存在，使用默认 MAC
    // 真实的初始化会在内核的 ne2000_init 中完成

    return 0;
}

// 发送数据包
static int ne2000_send(const uint8_t *data, uint32_t length)
{
    if (length > 1518)
        return -1;

    // 等待之前的传输完成
    int timeout = 10000;
    while ((inb(NE2000_CMD) & NE2000_CMD_TRANS) && timeout-- > 0)
        ;

    if (timeout <= 0)
        return -1;

    // 设置远程 DMA 写入
    outb(NE2000_CMD, NE2000_CMD_PAGE0 | NE2000_CMD_START | NE2000_CMD_NODMA);
    outb(NE2000_RBCR0, length & 0xFF);
    outb(NE2000_RBCR1, (length >> 8) & 0xFF);
    outb(NE2000_RSAR0, 0x00);
    outb(NE2000_RSAR1, 0x40); // 从 0x4000 开始

    outb(NE2000_CMD, NE2000_CMD_PAGE0 | NE2000_CMD_START | NE2000_CMD_RWRITE);

    // 写入数据
    for (uint32_t i = 0; i < length; i += 2)
    {
        uint16_t word = data[i] | (i + 1 < length ? data[i + 1] << 8 : 0);
        outb(NE2000_DATAPORT, word & 0xFF);
        outb(NE2000_DATAPORT, (word >> 8) & 0xFF);
    }

    // 设置传输参数
    outb(NE2000_TPSR, 0x40);
    outb(NE2000_TBCR0, length & 0xFF);
    outb(NE2000_TBCR1, (length >> 8) & 0xFF);

    // 开始传输
    outb(NE2000_CMD, NE2000_CMD_PAGE0 | NE2000_CMD_START | NE2000_CMD_TRANS | NE2000_CMD_NODMA);

    return 0;
}

// 接收数据包
static int ne2000_recv(uint8_t *buffer, uint32_t max_length)
{
    // 检查是否有数据包
    uint8_t curr;
    outb(NE2000_CMD, NE2000_CMD_PAGE1 | NE2000_CMD_NODMA);
    curr = inb(NE2000_CURR);
    outb(NE2000_CMD, NE2000_CMD_PAGE0 | NE2000_CMD_NODMA);

    if (next_packet == curr)
        return 0; // 没有数据包

    // 读取数据包头
    uint8_t header[4];
    outb(NE2000_RBCR0, 4);
    outb(NE2000_RBCR1, 0);
    outb(NE2000_RSAR0, 0);
    outb(NE2000_RSAR1, next_packet);
    outb(NE2000_CMD, NE2000_CMD_PAGE0 | NE2000_CMD_START | NE2000_CMD_RREAD);

    for (int i = 0; i < 4; i++)
        header[i] = inb(NE2000_DATAPORT);

    uint8_t next = header[1];
    uint16_t length = (header[3] << 8) | header[2];

    if (length > max_length || length < 60)
    {
        next_packet = next;
        outb(NE2000_BNRY, next_packet == rx_page_start ? rx_page_stop - 1 : next_packet - 1);
        return 0;
    }

    // 读取数据
    length -= 4; // 减去头部
    outb(NE2000_RBCR0, length & 0xFF);
    outb(NE2000_RBCR1, (length >> 8) & 0xFF);
    outb(NE2000_RSAR0, 4);
    outb(NE2000_RSAR1, next_packet);
    outb(NE2000_CMD, NE2000_CMD_PAGE0 | NE2000_CMD_START | NE2000_CMD_RREAD);

    for (uint32_t i = 0; i < length; i++)
        buffer[i] = inb(NE2000_DATAPORT);

    // 更新边界指针
    next_packet = next;
    outb(NE2000_BNRY, next_packet == rx_page_start ? rx_page_stop - 1 : next_packet - 1);

    return length;
}

// 处理网络设备请求
static void handle_request(struct ipc_message_user *msg, uint32_t my_port)
{
    if (msg->size < sizeof(netdev_request_t))
    {
        return;
    }

    netdev_request_t *req = (netdev_request_t *)msg->data;

    netdev_response_t resp;
    resp.request_id = req->request_id;
    resp.status = NETDEV_STATUS_OK;
    resp.bytes_transferred = 0;

    switch (req->operation)
    {
    case NETDEV_OP_SEND:
        if (ne2000_send((uint8_t *)req->buffer_addr, req->length) == 0)
        {
            resp.bytes_transferred = req->length;
        }
        else
        {
            resp.status = NETDEV_STATUS_ERROR;
        }
        break;

    case NETDEV_OP_RECV:
    {
        int received = ne2000_recv((uint8_t *)req->buffer_addr, req->length);
        if (received > 0)
        {
            resp.bytes_transferred = received;
        }
        else
        {
            resp.status = NETDEV_STATUS_TIMEOUT;
        }
        break;
    }

    case NETDEV_OP_GET_MAC:
        memcpy(resp.mac_addr, mac_addr, 6);
        break;

    case NETDEV_OP_SET_MAC:
        // 切换到页 1
        outb(NE2000_CMD, NE2000_CMD_PAGE1 | NE2000_CMD_STOP | NE2000_CMD_NODMA);
        for (int i = 0; i < 6; i++)
        {
            mac_addr[i] = req->mac_addr[i];
            outb(NE2000_PAR0 + i, mac_addr[i]);
        }
        // 返回页 0
        outb(NE2000_CMD, NE2000_CMD_PAGE0 | NE2000_CMD_START | NE2000_CMD_NODMA);
        break;

    default:
        resp.status = NETDEV_STATUS_INVALID;
        break;
    }

    // 发送响应
    syscall_ipc_send(my_port, msg->sender_port, &resp, sizeof(resp));
}

// 系统调用：yield
static inline void syscall_yield(void)
{
    __asm__ volatile("int $0x80" : : "a"(4)); // SYS_YIELD = 4
}

// 用户空间 NE2000 驱动主函数
void ne2000_driver_main(void)
{
    // 创建命名 IPC 端口
    int port = syscall_ipc_create_named_port(NETDEV_PORT_NAME);
    if (port < 0)
    {
        // 端口创建失败，让出 CPU 后退出
        // 使用 yield 而不是 hlt
        while (1)
        {
            syscall_yield();
        }
    }

    // 主循环：接收并处理请求
    while (1)
    {
        struct ipc_message_user msg;

        // 阻塞等待请求
        if (syscall_ipc_recv(port, &msg) != 0)
        {
            syscall_yield(); // 让出 CPU
            continue;
        }

        // 处理请求并发送响应
        handle_request(&msg, port);
    }
}
