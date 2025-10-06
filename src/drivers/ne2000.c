#include "ne2000.h"
#include "netif.h"
#include "port_io.h"
#include <stdint.h>

// 全局设备结构
static struct ne2000_device ne2k;

// 辅助函数：延迟
static void delay(int count)
{
    for (volatile int i = 0; i < count; i++)
        ;
}

// 从 NE2000 读取数据
static void ne2000_read_block(uint16_t offset, uint8_t *buffer, uint16_t length)
{
    // 清除远程 DMA 完成标志
    outb(ne2k.io_base + NE_ISR, NE_ISR_RDC);

    // 设置远程 DMA 读取
    outb(ne2k.io_base + NE_RBCR0, length & 0xFF);
    outb(ne2k.io_base + NE_RBCR1, (length >> 8) & 0xFF);
    outb(ne2k.io_base + NE_RSAR0, offset & 0xFF);
    outb(ne2k.io_base + NE_RSAR1, (offset >> 8) & 0xFF);
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_RREAD | NE_CMD_START);

    // 读取数据 (16 位模式)
    for (uint16_t i = 0; i < length; i += 2)
    {
        uint16_t word = inw(ne2k.io_base + NE_DATAPORT);
        buffer[i] = word & 0xFF;
        if (i + 1 < length)
            buffer[i + 1] = (word >> 8) & 0xFF;
    }

    // 等待远程 DMA 完成（带超时）
    int timeout = 10000;
    while (!(inb(ne2k.io_base + NE_ISR) & NE_ISR_RDC) && timeout--)
        ;
    outb(ne2k.io_base + NE_ISR, NE_ISR_RDC);
}

// 向 NE2000 写入数据
static void ne2000_write_block(uint16_t offset, const uint8_t *buffer, uint16_t length)
{
    // 清除远程 DMA 完成标志
    outb(ne2k.io_base + NE_ISR, NE_ISR_RDC);

    // 设置远程 DMA 写入
    outb(ne2k.io_base + NE_RBCR0, length & 0xFF);
    outb(ne2k.io_base + NE_RBCR1, (length >> 8) & 0xFF);
    outb(ne2k.io_base + NE_RSAR0, offset & 0xFF);
    outb(ne2k.io_base + NE_RSAR1, (offset >> 8) & 0xFF);
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_RWRITE | NE_CMD_START);

    // 写入数据 (16 位模式)
    for (uint16_t i = 0; i < length; i += 2)
    {
        uint16_t word = buffer[i];
        if (i + 1 < length)
            word |= (buffer[i + 1] << 8);
        outw(ne2k.io_base + NE_DATAPORT, word);
    }

    // 等待远程 DMA 完成（带超时）
    int timeout = 10000;
    while (!(inb(ne2k.io_base + NE_ISR) & NE_ISR_RDC) && timeout--)
        ;
    outb(ne2k.io_base + NE_ISR, NE_ISR_RDC);
}

// 初始化 NE2000
int ne2000_init(void)
{
    ne2k.io_base = NE2000_IO_BASE;
    ne2k.irq = NE2000_IRQ;
    ne2k.packets_sent = 0;
    ne2k.packets_received = 0;

    // 检测设备是否存在
    // 尝试读取一个寄存器，如果返回 0xFF 可能设备不存在
    uint8_t test = inb(ne2k.io_base + NE_CMD);
    if (test == 0xFF)
    {
        return -1; // 设备不存在
    }

    // 重置网卡
    uint8_t reset = inb(ne2k.io_base + NE_RESET);
    outb(ne2k.io_base + NE_RESET, reset);
    delay(10000);

    // 等待重置完成（带超时）
    int timeout = 10000;
    while (!(inb(ne2k.io_base + NE_ISR) & NE_ISR_RST) && timeout--)
    {
        delay(10);
    }

    if (timeout <= 0)
    {
        return -1; // 重置超时，设备可能不存在
    }

    outb(ne2k.io_base + NE_ISR, 0xFF); // 清除所有中断

    // 停止网卡
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_STOP | NE_CMD_NODMA);
    delay(1000);

    // 配置数据寄存器 (16 位模式，FIFO 8 字节)
    outb(ne2k.io_base + NE_DCR, NE_DCR_WTS | NE_DCR_FT1);

    // 清除远程字节计数
    outb(ne2k.io_base + NE_RBCR0, 0);
    outb(ne2k.io_base + NE_RBCR1, 0);

    // 配置接收 (接受广播)
    outb(ne2k.io_base + NE_RCR, NE_RCR_AB);

    // 配置发送 (正常模式)
    outb(ne2k.io_base + NE_TCR, 0);

    // 配置接收缓冲区
    outb(ne2k.io_base + NE_PSTART, NE_RXBUF_START);
    outb(ne2k.io_base + NE_PSTOP, NE_RXBUF_END);
    outb(ne2k.io_base + NE_BOUNDARY, NE_RXBUF_START);

    // 清除中断状态
    outb(ne2k.io_base + NE_ISR, 0xFF);

    // 切换到页 1 读取 MAC 地址
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE1 | NE_CMD_STOP | NE_CMD_NODMA);

    // 从页 1 的 PAR 寄存器读取 MAC 地址
    // QEMU 会在这里预设 MAC 地址
    for (int i = 0; i < 6; i++)
    {
        ne2k.mac_addr[i] = inb(ne2k.io_base + NE_PAR0 + i);
    }

    // 如果读取失败（全零），使用默认 MAC 地址
    int all_zero = 1;
    for (int i = 0; i < 6; i++)
    {
        if (ne2k.mac_addr[i] != 0)
        {
            all_zero = 0;
            break;
        }
    }

    if (all_zero)
    {
        // 使用 QEMU 默认 MAC 地址格式: 52:54:00:12:34:56
        ne2k.mac_addr[0] = 0x52;
        ne2k.mac_addr[1] = 0x54;
        ne2k.mac_addr[2] = 0x00;
        ne2k.mac_addr[3] = 0x12;
        ne2k.mac_addr[4] = 0x34;
        ne2k.mac_addr[5] = 0x56;
    }

    // 保持在页 1，确保 MAC 地址已写入物理地址寄存器
    for (int i = 0; i < 6; i++)
    {
        outb(ne2k.io_base + NE_PAR0 + i, ne2k.mac_addr[i]);
    }

    // 设置当前页指针
    outb(ne2k.io_base + NE_CURR, NE_RXBUF_START + 1);
    ne2k.current_page = NE_RXBUF_START + 1;

    // 回到页 0
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_NODMA);

    // 启用中断 (接收、发送、溢出)
    outb(ne2k.io_base + NE_IMR, NE_ISR_PRX | NE_ISR_PTX | NE_ISR_OVW);

    // 启动网卡
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_START | NE_CMD_NODMA);

    // 设置发送配置为正常模式
    outb(ne2k.io_base + NE_TCR, 0);

    return 0;
}

// 发送数据包
int ne2000_send(const uint8_t *data, uint16_t length)
{
    if (!data)
        return -1;

    if (length < 60)
        length = 60; // 最小以太网帧大小
    if (length > 1518)
        return -1; // 超过最大帧大小

    // 等待前一个发送完成
    int timeout = 100000;
    while ((inb(ne2k.io_base + NE_CMD) & NE_CMD_TRANSMIT) && timeout--)
        delay(1);

    if (timeout <= 0)
        return -1; // 超时

    // 确保在页0
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_START | NE_CMD_NODMA);

    // 清除远程 DMA 完成标志
    outb(ne2k.io_base + NE_ISR, NE_ISR_RDC);

    // 设置远程 DMA 写入到发送缓冲区
    outb(ne2k.io_base + NE_RSAR0, 0);
    outb(ne2k.io_base + NE_RSAR1, NE_TXBUF_START);
    outb(ne2k.io_base + NE_RBCR0, length & 0xFF);
    outb(ne2k.io_base + NE_RBCR1, (length >> 8) & 0xFF);
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_RWRITE | NE_CMD_START);

    // 写入数据 (16 位模式)
    for (uint16_t i = 0; i < length; i += 2)
    {
        uint16_t word = data[i];
        if (i + 1 < length)
            word |= (data[i + 1] << 8);
        outw(ne2k.io_base + NE_DATAPORT, word);
    }

    // 等待远程 DMA 完成
    timeout = 10000;
    while (!(inb(ne2k.io_base + NE_ISR) & NE_ISR_RDC) && timeout--)
        delay(1);
    outb(ne2k.io_base + NE_ISR, NE_ISR_RDC);

    // 设置发送页和字节计数
    outb(ne2k.io_base + NE_TPSR, NE_TXBUF_START);
    outb(ne2k.io_base + NE_TBCR0, length & 0xFF);
    outb(ne2k.io_base + NE_TBCR1, (length >> 8) & 0xFF);

    // 启动发送
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_START | NE_CMD_TRANSMIT | NE_CMD_NODMA);

    ne2k.packets_sent++;
    return 0;
}

// 接收数据包
int ne2000_receive(uint8_t *buffer, uint16_t max_length)
{
    if (!buffer || max_length == 0)
        return 0;

    // 检查是否有数据包
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_START | NE_CMD_NODMA);
    uint8_t boundary = inb(ne2k.io_base + NE_BOUNDARY);

    // 切换到页 1 读取当前页
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE1 | NE_CMD_START | NE_CMD_NODMA);
    uint8_t current = inb(ne2k.io_base + NE_CURR);
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_START | NE_CMD_NODMA);

    if (boundary == current)
        return 0; // 没有数据包

    // 读取数据包头
    struct ne2000_rx_header header;
    uint16_t offset = (boundary + 1) * NE_PAGE_SIZE;

    // 使用简化的读取，避免调用可能有问题的 ne2000_read_block
    // 直接读取头部
    outb(ne2k.io_base + NE_ISR, NE_ISR_RDC);
    outb(ne2k.io_base + NE_RSAR0, offset & 0xFF);
    outb(ne2k.io_base + NE_RSAR1, (offset >> 8) & 0xFF);
    outb(ne2k.io_base + NE_RBCR0, sizeof(header));
    outb(ne2k.io_base + NE_RBCR1, 0);
    outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_RREAD | NE_CMD_START);

    // 读取头部
    uint16_t *header_ptr = (uint16_t *)&header;
    for (unsigned int i = 0; i < sizeof(header) / 2; i++)
    {
        header_ptr[i] = inw(ne2k.io_base + NE_DATAPORT);
    }

    // 验证数据包大小
    if (header.count < sizeof(header) || header.count > 1518)
        return 0; // 无效数据包

    uint16_t data_length = header.count - sizeof(header);
    if (data_length > max_length)
        data_length = max_length;

    // 读取数据包内容（如果有）
    if (data_length > 0)
    {
        outb(ne2k.io_base + NE_ISR, NE_ISR_RDC);
        outb(ne2k.io_base + NE_RSAR0, (offset + sizeof(header)) & 0xFF);
        outb(ne2k.io_base + NE_RSAR1, ((offset + sizeof(header)) >> 8) & 0xFF);
        outb(ne2k.io_base + NE_RBCR0, data_length & 0xFF);
        outb(ne2k.io_base + NE_RBCR1, (data_length >> 8) & 0xFF);
        outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_RREAD | NE_CMD_START);

        for (uint16_t i = 0; i < data_length; i += 2)
        {
            uint16_t word = inw(ne2k.io_base + NE_DATAPORT);
            buffer[i] = word & 0xFF;
            if (i + 1 < data_length)
                buffer[i + 1] = (word >> 8) & 0xFF;
        }
    }

    // 更新边界指针
    uint8_t next_boundary;
    if (header.next_page >= NE_RXBUF_END)
        next_boundary = NE_RXBUF_START;
    else if (header.next_page == NE_RXBUF_START)
        next_boundary = NE_RXBUF_END - 1;
    else
        next_boundary = header.next_page - 1;

    outb(ne2k.io_base + NE_BOUNDARY, next_boundary);

    ne2k.packets_received++;
    return data_length;
}

// 获取 MAC 地址
void ne2000_get_mac_address(uint8_t *mac)
{
    for (int i = 0; i < 6; i++)
        mac[i] = ne2k.mac_addr[i];
}

// 中断处理程序
void ne2000_irq_handler(void)
{
    // 读取中断状态
    uint8_t isr = inb(ne2k.io_base + NE_ISR);

    // 清除中断
    outb(ne2k.io_base + NE_ISR, isr);

    // 处理接收中断
    if (isr & NE_ISR_PRX)
    {
        // 数据包接收完成
        // 在实际系统中，这里会通知网络栈
    }

    // 处理发送中断
    if (isr & NE_ISR_PTX)
    {
        // 数据包发送完成
    }

    // 处理溢出
    if (isr & NE_ISR_OVW)
    {
        // 缓冲区溢出，需要重置接收器
        outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_STOP | NE_CMD_NODMA);
        delay(1000);
        outb(ne2k.io_base + NE_CMD, NE_CMD_PAGE0 | NE_CMD_START | NE_CMD_NODMA);
    }
}

// NE2000 网络接口操作
struct netif_ops ne2000_ops = {
    .send = ne2000_send,
    .receive = ne2000_receive,
    .get_mac = ne2000_get_mac_address,
};
