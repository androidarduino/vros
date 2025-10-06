/*
 * 用户空间 ATA 驱动
 *
 * 这是一个运行在用户空间的块设备驱动程序
 * 通过 IPC 与内核通信，使用 I/O 端口直接访问硬件
 */

#include <stdint.h>
#include "blkdev_ipc.h"
#include "ata.h"
#include "syscall.h"

// 系统调用包装函数
static inline int syscall_request_io_port(uint16_t port_start, uint16_t port_end)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_REQUEST_IO_PORT), "b"(port_start), "c"(port_end));
    return ret;
}

static inline int syscall_register_irq_handler(uint8_t irq, uint32_t ipc_port)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_REGISTER_IRQ_HANDLER), "b"(irq), "c"(ipc_port));
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

// IPC 消息结构（与内核定义匹配）
struct ipc_message_user
{
    uint32_t sender_pid;
    uint32_t sender_port;
    uint32_t type;
    uint32_t size;
    char data[256];
};

static inline int syscall_ipc_recv(uint32_t port, struct ipc_message_user *msg)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_RECV), "b"(port), "c"(msg));
    return ret;
}

static inline int syscall_ipc_send(uint32_t src_port, uint32_t dst_port, const void *data, uint32_t size)
{
    // 使用 ipc_send 系统调用
    // sys_ipc_send(dest_port, type, data, size)
    int ret;
    (void)src_port; // 暂时不使用
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_SEND), "b"(dst_port), "c"(0), "d"(data), "S"(size));
    return ret;
}

static inline int syscall_write(int fd, const char *buf, int len)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len));
    return ret;
}

// I/O 端口访问（需要 I/O 权限）
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

// 简单的字符串输出
static void print(const char *str)
{
    int len = 0;
    while (str[len])
        len++;
    syscall_write(1, str, len);
}

// 简单的内存拷贝
static void *memcpy(void *dest, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--)
        *d++ = *s++;
    return dest;
}

// ATA 寄存器地址
#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_PRIMARY_IRQ 14

// ATA 寄存器偏移
#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_FEATURES 1
#define ATA_REG_SECCOUNT 2
#define ATA_REG_LBA_LO 3
#define ATA_REG_LBA_MID 4
#define ATA_REG_LBA_HI 5
#define ATA_REG_DRIVE 6
#define ATA_REG_STATUS 7
#define ATA_REG_COMMAND 7

// ATA 命令
#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_FLUSH 0xE7
#define ATA_CMD_IDENTIFY 0xEC

// ATA 状态位
#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

// 等待 ATA 就绪
static int ata_wait_ready(void)
{
    uint32_t timeout = 1000000;
    while (timeout--)
    {
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY))
            return 0;
    }
    return -1;
}

// 等待 ATA 数据就绪
static int ata_wait_drq(void)
{
    uint32_t timeout = 1000000;
    while (timeout--)
    {
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status & ATA_SR_DRQ)
            return 0;
        if (status & ATA_SR_ERR)
            return -1;
    }
    return -1;
}

// 读取扇区
static int ata_read_sector(uint8_t drive, uint32_t lba, uint16_t *buffer)
{
    if (ata_wait_ready() != 0)
        return -1;

    // 设置驱动器和 LBA
    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 1);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, lba & 0xFF);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);

    // 发送读取命令
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    // 等待数据就绪
    if (ata_wait_drq() != 0)
        return -1;

    // 读取数据
    for (int i = 0; i < 256; i++)
        buffer[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);

    return 0;
}

// 写入扇区
static int ata_write_sector(uint8_t drive, uint32_t lba, const uint16_t *buffer)
{
    if (ata_wait_ready() != 0)
        return -1;

    // 设置驱动器和 LBA
    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 1);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, lba & 0xFF);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);

    // 发送写入命令
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    // 等待数据就绪
    if (ata_wait_drq() != 0)
        return -1;

    // 写入数据
    for (int i = 0; i < 256; i++)
        outw(ATA_PRIMARY_IO + ATA_REG_DATA, buffer[i]);

    // 刷新缓存
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_FLUSH);
    ata_wait_ready();

    return 0;
}

// 处理块设备请求
static void handle_request(struct ipc_message_user *msg, uint32_t my_port)
{
    // 从消息中提取请求
    if (msg->size < sizeof(blkdev_request_t))
    {
        return;
    }

    blkdev_request_t *req = (blkdev_request_t *)msg->data;

    blkdev_response_t resp;
    resp.request_id = req->request_id;
    resp.status = BLKDEV_STATUS_OK;
    resp.bytes_transferred = 0;

    uint16_t sector_buffer[256];

    switch (req->operation)
    {
    case BLKDEV_OP_READ:
        for (uint32_t i = 0; i < req->count; i++)
        {
            if (ata_read_sector(req->drive, req->lba + i, sector_buffer) != 0)
            {
                resp.status = BLKDEV_STATUS_ERROR;
                break;
            }
            // 拷贝到请求的缓冲区
            memcpy((void *)(req->buffer_addr + i * 512), sector_buffer, 512);
            resp.bytes_transferred += 512;
        }
        break;

    case BLKDEV_OP_WRITE:
        for (uint32_t i = 0; i < req->count; i++)
        {
            // 从请求的缓冲区拷贝
            memcpy(sector_buffer, (void *)(req->buffer_addr + i * 512), 512);
            if (ata_write_sector(req->drive, req->lba + i, sector_buffer) != 0)
            {
                resp.status = BLKDEV_STATUS_ERROR;
                break;
            }
            resp.bytes_transferred += 512;
        }
        break;

    case BLKDEV_OP_FLUSH:
        outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_FLUSH);
        if (ata_wait_ready() != 0)
            resp.status = BLKDEV_STATUS_ERROR;
        break;

    default:
        resp.status = BLKDEV_STATUS_INVALID;
        break;
    }

    // 发送响应到发送者的端口（sender_port 可以是 0，这是有效的端口）
    syscall_ipc_send(my_port, msg->sender_port, &resp, sizeof(resp));
}

// 用户空间 ATA 驱动主函数
void ata_driver_main(void)
{
    // 1. 请求 I/O 端口访问权限
    if (syscall_request_io_port(ATA_PRIMARY_IO, ATA_PRIMARY_IO + 7) != 0)
    {
        return;
    }
    if (syscall_request_io_port(ATA_PRIMARY_CTRL, ATA_PRIMARY_CTRL) != 0)
    {
        return;
    }

    // 2. 创建命名 IPC 端口
    int port = syscall_ipc_create_named_port(BLKDEV_PORT_NAME);
    if (port < 0)
    {
        return;
    }

    // 3. 注册 IRQ 处理器（可选）
    // syscall_register_irq_handler(ATA_PRIMARY_IRQ, port);

    // 4. 主循环：接收并处理请求
    while (1)
    {
        struct ipc_message_user msg;

        // 阻塞等待请求
        if (syscall_ipc_recv(port, &msg) != 0)
            continue;

        // 处理请求并发送响应到 sender_port
        handle_request(&msg, port);
    }
}
