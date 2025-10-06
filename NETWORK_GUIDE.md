# VROS 网络驱动指南

## 📡 NE2000 网络驱动

VROS 现已支持 NE2000 ISA 网络适配器！这是一个经典的以太网控制器，在 QEMU 中有完美的支持。

### 硬件配置

- **I/O 基地址**: 0x300
- **IRQ**: 11
- **工作模式**: ISA 总线
- **传输模式**: 16 位 DMA

### 功能特性

✅ **已实现**:
- NE2000 硬件初始化
- MAC 地址读取
- 数据包发送（支持最大 1518 字节以太网帧）
- 数据包接收（支持完整缓冲区管理）
- 中断处理（接收、发送、溢出）
- 网络接口抽象层

### Shell 命令

#### 1. `ifconfig` - 显示网络接口信息

显示所有网络接口的状态和统计信息：

```bash
> ifconfig
```

输出示例：
```
=== Network Interfaces ===
Interface: eth0
  MAC Address: 52:54:00:12:34:56
  TX packets: 10  bytes: 600
  RX packets: 5   bytes: 300
  Errors: 0
```

#### 2. `nettest` - 测试网络收发功能

发送一个 ARP 广播请求并尝试接收响应：

```bash
> nettest
```

这个测试命令会：
1. 构建一个标准的 ARP 请求数据包
2. 广播到网络（目标 IP: 192.168.1.1）
3. 监听 5 次，尝试接收响应
4. 显示接收到的数据包内容（前 32 字节）

### QEMU 网络配置

#### 用户模式网络（默认）

Makefile 已配置为用户模式网络：

```bash
qemu-system-x86_64 -kernel kernel.bin \
  -drive file=disk.img,format=raw,index=0,media=disk \
  -netdev user,id=net0 \
  -device ne2k_isa,netdev=net0,iobase=0x300,irq=11
```

**特点**：
- 无需 root 权限
- QEMU 自动提供 DHCP、DNS
- 主机 <-> 虚拟机 NAT 连接
- 虚拟机 IP: 通常是 10.0.2.15
- 网关: 10.0.2.2

#### TAP 网络模式（高级）

如需更真实的网络环境：

```bash
qemu-system-x86_64 -kernel kernel.bin \
  -drive file=disk.img,format=raw,index=0,media=disk \
  -netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
  -device ne2k_isa,netdev=net0,iobase=0x300,irq=11
```

### 技术细节

#### NE2000 寄存器

| 寄存器 | 偏移 | 功能 |
|--------|------|------|
| CMD    | 0x00 | 命令寄存器 |
| DATAPORT | 0x10 | 数据端口 (16位传输) |
| RESET  | 0x1F | 重置端口 |
| PSTART | 0x01 | 接收缓冲区起始页 |
| PSTOP  | 0x02 | 接收缓冲区结束页 |
| ISR    | 0x07 | 中断状态寄存器 |
| IMR    | 0x0F | 中断屏蔽寄存器 |

#### 内存布局

```
0x4000 (页 0x40) - 0x4600  发送缓冲区 (6 页 = 1536 字节)
0x4600 (页 0x46) - 0x8000  接收缓冲区环形队列 (58 页)
```

#### 数据包接收流程

1. 网卡接收到数据包
2. 将数据写入接收缓冲区的当前页
3. 触发 IRQ 11 中断
4. 驱动读取数据包头（包含状态、下一页指针、长度）
5. 从缓冲区复制数据到用户空间
6. 更新 BOUNDARY 寄存器

#### 数据包发送流程

1. 用户空间调用 `netif_send()`
2. 驱动将数据复制到发送缓冲区（页 0x40）
3. 设置 TPSR（发送页起始）和 TBCR（字节计数）
4. 发送 TRANSMIT 命令
5. 等待发送完成中断

### 代码架构

```
src/include/ne2000.h        - NE2000 驱动接口
src/drivers/ne2000.c        - NE2000 驱动实现
src/include/netif.h         - 网络接口抽象层
src/drivers/netif.c         - 网络接口管理
```

#### 网络接口抽象层

```c
struct netif_ops {
    int (*send)(const uint8_t *data, uint16_t length);
    int (*receive)(uint8_t *buffer, uint16_t max_length);
    void (*get_mac)(uint8_t *mac);
};
```

这个设计允许将来添加其他网络驱动（如 RTL8139、E1000），而不需要修改上层代码。

### 调试技巧

#### 1. 使用 Wireshark 监听

在主机上监听 QEMU 网络流量：

```bash
# 对于用户模式网络
sudo tcpdump -i any -n 'udp port 67 or arp'

# 对于 TAP 模式
sudo tcpdump -i tap0
```

#### 2. 查看网络统计

```bash
> ifconfig
```

检查 TX/RX 计数器和错误计数。

#### 3. NE2000 寄存器诊断

在 `ne2000.c` 中添加调试输出：

```c
uint8_t cmd = inb(ne2k.io_base + NE_CMD);
uint8_t isr = inb(ne2k.io_base + NE_ISR);
// 打印寄存器状态
```

### 下一步计划

🚀 **可扩展的网络功能**：

1. **ARP 协议实现** - 地址解析
2. **IP 协议栈** - IPv4 支持
3. **ICMP** - ping 命令
4. **UDP** - 简单数据报传输
5. **TCP** - 可靠连接
6. **Socket API** - 用户空间网络编程接口
7. **DHCP 客户端** - 自动 IP 配置
8. **DNS 解析器** - 域名查询

### 常见问题

**Q: 为什么 nettest 没有收到响应？**

A: 在用户模式网络下，ARP 请求通常不会收到真实的响应，因为 QEMU 虚拟网络不会完全模拟 ARP。这是正常的。要测试真实的网络交互，需要实现 IP/ICMP 协议。

**Q: 如何更改 MAC 地址？**

A: NE2000 的 MAC 地址由 QEMU 自动分配。可以在启动 QEMU 时指定：

```bash
-device ne2k_isa,netdev=net0,iobase=0x300,irq=11,mac=52:54:00:12:34:56
```

**Q: 支持其他网络适配器吗？**

A: 目前仅支持 NE2000。但网络接口抽象层已就绪，添加新驱动（如 RTL8139）只需实现 `netif_ops` 接口。

### 性能指标

- **最大吞吐量**: ~10 Mbps（理论，NE2000 硬件限制）
- **最小帧大小**: 60 字节（以太网标准）
- **最大帧大小**: 1518 字节（以太网标准）
- **接收缓冲区**: 14.5 KB（58 页）
- **发送缓冲区**: 1.5 KB（6 页）

### 参考资料

- [NE2000 规格说明](http://www.national.com/ds/DP/DP8390D.pdf)
- [以太网帧格式](https://en.wikipedia.org/wiki/Ethernet_frame)
- [QEMU 网络配置](https://wiki.qemu.org/Documentation/Networking)

---

**恭喜！** 🎉 您的微内核操作系统现在已经可以与外部世界通信了！

