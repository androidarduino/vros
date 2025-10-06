#ifndef NE2000_H
#define NE2000_H

#include <stdint.h>

// NE2000 默认端口和中断
#define NE2000_IO_BASE 0x300
#define NE2000_IRQ 11

// NE2000 寄存器偏移
#define NE_CMD      0x00    // 命令寄存器
#define NE_DATAPORT 0x10    // 数据端口
#define NE_RESET    0x1F    // 重置端口

// Page 0 寄存器
#define NE_PSTART   0x01    // 页起始地址
#define NE_PSTOP    0x02    // 页停止地址
#define NE_BOUNDARY 0x03    // 边界指针
#define NE_TSR      0x04    // 发送状态寄存器
#define NE_TPSR     0x04    // 发送页起始地址
#define NE_TBCR0    0x05    // 发送字节计数 (低)
#define NE_TBCR1    0x06    // 发送字节计数 (高)
#define NE_ISR      0x07    // 中断状态寄存器
#define NE_RSAR0    0x08    // 远程起始地址 (低)
#define NE_RSAR1    0x09    // 远程起始地址 (高)
#define NE_RBCR0    0x0A    // 远程字节计数 (低)
#define NE_RBCR1    0x0B    // 远程字节计数 (高)
#define NE_RSR      0x0C    // 接收状态寄存器
#define NE_RCR      0x0C    // 接收配置寄存器
#define NE_TCR      0x0D    // 发送配置寄存器
#define NE_DCR      0x0E    // 数据配置寄存器
#define NE_IMR      0x0F    // 中断屏蔽寄存器

// Page 1 寄存器
#define NE_PAR0     0x01    // 物理地址 (6 字节)
#define NE_CURR     0x07    // 当前页寄存器
#define NE_MAR0     0x08    // 多播地址 (8 字节)

// 命令寄存器位
#define NE_CMD_STOP     0x01    // 停止
#define NE_CMD_START    0x02    // 启动
#define NE_CMD_TRANSMIT 0x04    // 发送
#define NE_CMD_RREAD    0x08    // 远程读
#define NE_CMD_RWRITE   0x10    // 远程写
#define NE_CMD_NODMA    0x20    // 无 DMA
#define NE_CMD_PAGE0    0x00    // 选择页 0
#define NE_CMD_PAGE1    0x40    // 选择页 1
#define NE_CMD_PAGE2    0x80    // 选择页 2

// 中断状态/屏蔽位
#define NE_ISR_PRX  0x01    // 数据包接收
#define NE_ISR_PTX  0x02    // 数据包发送
#define NE_ISR_RXE  0x04    // 接收错误
#define NE_ISR_TXE  0x08    // 发送错误
#define NE_ISR_OVW  0x10    // 溢出
#define NE_ISR_CNT  0x20    // 计数器溢出
#define NE_ISR_RDC  0x40    // 远程 DMA 完成
#define NE_ISR_RST  0x80    // 重置状态

// 接收配置寄存器位
#define NE_RCR_SEP  0x01    // 保存错误数据包
#define NE_RCR_AR   0x02    // 接受不到 8 字节的数据包
#define NE_RCR_AB   0x04    // 接受广播
#define NE_RCR_AM   0x08    // 接受多播
#define NE_RCR_PRO  0x10    // 混杂模式
#define NE_RCR_MON  0x20    // 监视模式

// 发送配置寄存器位
#define NE_TCR_CRC  0x01    // 禁用 CRC
#define NE_TCR_LB0  0x02    // 环回模式位 0
#define NE_TCR_LB1  0x04    // 环回模式位 1
#define NE_TCR_ATD  0x08    // 自动发送禁用

// 数据配置寄存器位
#define NE_DCR_WTS  0x01    // 字传输模式
#define NE_DCR_BOS  0x02    // 字节顺序
#define NE_DCR_LAS  0x04    // 长地址
#define NE_DCR_LS   0x08    // 环回选择
#define NE_DCR_ARM  0x10    // 自动初始化远程
#define NE_DCR_FT0  0x20    // FIFO 阈值位 0
#define NE_DCR_FT1  0x40    // FIFO 阈值位 1

// 接收缓冲区配置
#define NE_PAGE_SIZE    256     // 每页字节数
#define NE_TXBUF_START  0x40    // 发送缓冲区起始页 (16KB)
#define NE_TXBUF_SIZE   6       // 发送缓冲区页数
#define NE_RXBUF_START  0x46    // 接收缓冲区起始页
#define NE_RXBUF_END    0x80    // 接收缓冲区结束页 (32KB)

// 接收包头结构
struct ne2000_rx_header
{
    uint8_t status;      // 接收状态
    uint8_t next_page;   // 下一页指针
    uint16_t count;      // 字节计数
} __attribute__((packed));

// NE2000 设备结构
struct ne2000_device
{
    uint16_t io_base;           // I/O 基地址
    uint8_t irq;                // 中断号
    uint8_t mac_addr[6];        // MAC 地址
    uint8_t current_page;       // 当前页
    uint32_t packets_sent;      // 发送的数据包数
    uint32_t packets_received;  // 接收的数据包数
};

// 函数声明
int ne2000_init(void);
int ne2000_send(const uint8_t *data, uint16_t length);
int ne2000_receive(uint8_t *buffer, uint16_t max_length);
void ne2000_get_mac_address(uint8_t *mac);
void ne2000_irq_handler(void);

#endif // NE2000_H

