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
#define SYS_YIELD 4
#define SYS_IPC_CREATE_PORT 9
#define SYS_IPC_SEND 10
#define SYS_IPC_RECV 11
#define SYS_IPC_CREATE_NAMED_PORT 12
#define SYS_IPC_FIND_PORT 13
#define SYS_IPC_TRY_RECV 14
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
static inline void syscall_write(const char *msg, int len)
{
    __asm__ volatile("int $0x80" : : "a"(1), "b"(1), "c"(msg), "d"(len)); // SYS_WRITE = 1
}

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

static inline int syscall_ipc_send(uint32_t dest_port, uint32_t type, const void *data, uint32_t size)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_SEND), "b"(dest_port), "c"(type), "d"(data), "S"(size));
    return ret;
}

static inline int syscall_ipc_try_recv(uint32_t port, struct ipc_message_user *msg)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_TRY_RECV), "b"(port), "c"(msg));
    return ret;
}

static inline int syscall_ipc_find_port(const char *name)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_FIND_PORT), "b"(name));
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

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// memcpy 辅助函数
static void *memcpy(void *dest, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

// 全局变量
static uint8_t mac_addr[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint8_t rx_page_start = 0x46;
static uint8_t rx_page_stop = 0x80;
static uint8_t next_packet = 0x46;

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

    // 写入数据（使用16位输出）
    for (uint32_t i = 0; i < length; i += 2)
    {
        uint16_t word = data[i] | (i + 1 < length ? data[i + 1] << 8 : 0);
        outw(NE2000_DATAPORT, word); // 使用 outw 一次性写入16位
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

    // 计算接收地址（按页计算）
    uint16_t start_addr = next_packet << 8; // 页号 * 256

    // 读取数据包头（4字节）
    outb(NE2000_RBCR0, 4);
    outb(NE2000_RBCR1, 0);
    outb(NE2000_RSAR0, start_addr & 0xFF);
    outb(NE2000_RSAR1, (start_addr >> 8) & 0xFF);
    outb(NE2000_CMD, NE2000_CMD_PAGE0 | NE2000_CMD_START | NE2000_CMD_RREAD);

    // 使用 inw 读取头部
    uint16_t word1 = inw(NE2000_DATAPORT);
    uint16_t word2 = inw(NE2000_DATAPORT);
    uint8_t status = word1 & 0xFF;
    uint8_t next = (word1 >> 8) & 0xFF;
    uint16_t length = word2; // 已经是小端序

    (void)status; // 避免未使用警告

    // ARP包可能只有46字节（含4字节头部），所以最小值应该是18（14字节以太网头+4字节NE2000头）
    if (length < 18 || length > max_length)
    {
        next_packet = next;
        outb(NE2000_BNRY, next_packet == rx_page_start ? rx_page_stop - 1 : next_packet - 1);
        return 0;
    }

    // 读取实际数据（从头部后开始，跳过4字节头部）
    uint16_t data_len = length - 4; // 减去4字节头部
    uint16_t data_addr = start_addr + 4;

    outb(NE2000_RBCR0, data_len & 0xFF);
    outb(NE2000_RBCR1, (data_len >> 8) & 0xFF);
    outb(NE2000_RSAR0, data_addr & 0xFF);
    outb(NE2000_RSAR1, (data_addr >> 8) & 0xFF);
    outb(NE2000_CMD, NE2000_CMD_PAGE0 | NE2000_CMD_START | NE2000_CMD_RREAD);

    // 使用 inw 读取16位数据
    for (uint32_t i = 0; i < data_len; i += 2)
    {
        uint16_t word = inw(NE2000_DATAPORT);
        buffer[i] = word & 0xFF;
        if (i + 1 < data_len)
            buffer[i + 1] = (word >> 8) & 0xFF;
    }

    // 更新边界指针
    next_packet = next;
    outb(NE2000_BNRY, next_packet == rx_page_start ? rx_page_stop - 1 : next_packet - 1);

    return data_len;
}

// 处理网络设备请求
static void handle_request(struct ipc_message_user *msg)
{
    // 检查是否是直接发送数据包（来自 netstack）
    // type = 2 表示直接发送，数据包在 msg->data 中
    if (msg->type == 2)
    {
        // 直接发送数据包
        if (msg->size > 0 && msg->size <= 1518)
            ne2000_send((uint8_t *)msg->data, msg->size);
        // 不需要回复
        return;
    }

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
    // 参数: dest_port, type, data, size
    syscall_ipc_send(msg->sender_port, 0, &resp, sizeof(resp));
}

// 系统调用：yield
static inline void syscall_yield(void)
{
    __asm__ volatile("int $0x80" : : "a"(4)); // SYS_YIELD = 4
}

// 用户空间 NE2000 驱动主函数
void ne2000_driver_main(void)
{
    // 请求 I/O 端口权限（NE2000 使用 0x300-0x31F）
    if (syscall_request_io_port(0x300, 0x31F) < 0)
    {
        while (1)
            syscall_yield();
    }

    // 创建命名 IPC 端口
    int port = syscall_ipc_create_named_port(NETDEV_PORT_NAME);
    if (port < 0)
    {
        while (1)
            syscall_yield();
    }

    // 确保NE2000处于接收模式
    // RCR: 接受广播 + 接受多播 + 接受所有帧 (promiscuous)
    outb(NE2000_RCR, 0x04 | 0x08 | 0x10); // AB | AM | PRO

    // 清除中断状态
    outb(NE2000_ISR, 0xFF);

    // 查找 netstack 端口（用于转发接收到的数据包）
    int netstack_port = -1;
    int poll_count = 0;

    // 主循环：接收并处理请求 + 轮询网络数据包
    while (1)
    {
        struct ipc_message_user msg;

        // 使用非阻塞接收，避免阻塞数据包接收
        if (syscall_ipc_try_recv(port, &msg) == 0)
            handle_request(&msg);

        // 每 100 次循环查找一次 netstack（如果还没找到）
        poll_count++;
        if (netstack_port < 0 && (poll_count % 100) == 0)
            netstack_port = syscall_ipc_find_port("net.stack");

        // 检查是否有新的网络数据包（轮询）
        uint8_t isr = inb(NE2000_ISR);

        // 如果有接收中断标志（PRX）
        if (isr & 0x01)
        {
            // 清除中断标志
            outb(NE2000_ISR, 0x01);

            // 读取接收到的数据包
            uint8_t packet_buffer[1518]; // 最大以太网帧大小
            int packet_len = ne2000_recv(packet_buffer, sizeof(packet_buffer));

            // 如果成功接收到数据包，并且找到了 netstack，转发数据包
            if (packet_len > 0 && netstack_port >= 0)
            {
                // type = 1 表示接收到的网络数据包
                syscall_ipc_send(netstack_port, 1, packet_buffer, packet_len);
            }
        }

        syscall_yield(); // 让出 CPU
    }
}
