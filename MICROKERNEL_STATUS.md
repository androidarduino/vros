# VROS 微内核改造进度

## ✅ 已完成的基础设施

### 1. I/O 端口权限系统
- **文件**: `src/kernel/ioport.c`, `src/include/ioport.h`
- **功能**: 允许用户空间进程访问特定的 I/O 端口
- **系统调用**: `sys_request_io_port(port_start, port_end)`
- **实现**: 基于 TSS 中的 I/O Permission Bitmap (IOPB)
- **每个任务**: 可以有独立的 8192 字节 IOPB

### 2. IRQ 到 IPC 桥接
- **文件**: `src/kernel/irq_bridge.c`, `src/include/irq_bridge.h`
- **功能**: 将硬件中断转发到用户空间进程
- **系统调用**: `sys_register_irq_handler(irq, ipc_port)`
- **机制**: 
  - 用户进程注册一个 IPC 端口来接收 IRQ
  - 当 IRQ 发生时，内核发送 IPC 消息到该端口
  - 消息类型: `IPC_MSG_IRQ` (0x1000)
  - 消息内容: IRQ 号、时间戳

### 3. 任务结构更新
- 添加了 `uint8_t *iopb` 字段到 `struct task`
- 每个任务可以有独立的 I/O 权限

### 4. 新系统调用
```c
// 系统调用 15
int sys_request_io_port(uint16_t port_start, uint16_t port_end);

// 系统调用 16
int sys_register_irq_handler(uint8_t irq, uint32_t ipc_port);
```

## 🔄 下一步：用户空间驱动

### 方案 A：简化版（推荐先实现）
创建用户空间驱动演示，展示基本原理：

1. **简单的用户空间 ATA 驱动**
   - 创建 `src/lib/ata_driver_user.c`
   - 通过 IPC 提供读写服务
   - 使用 `sys_request_io_port()` 获取 ATA 端口权限
   - 使用 `sys_register_irq_handler()` 接收 IRQ 14/15

2. **简单的用户空间网络驱动**
   - 创建 `src/lib/ne2000_driver_user.c`
   - 通过 IPC 提供网络服务
   - 使用 `sys_request_io_port()` 获取 NE2000 端口权限 (0x300-0x31F)
   - 使用 `sys_register_irq_handler()` 接收 IRQ 11

3. **服务协议**
   ```c
   // ATA 服务消息类型
   #define ATA_MSG_READ  0x2001
   #define ATA_MSG_WRITE 0x2002
   
   // 网络服务消息类型
   #define NET_MSG_SEND    0x3001
   #define NET_MSG_RECEIVE 0x3002
   ```

### 方案 B：完整版（更复杂，需要更多时间）
完全重构块设备和网络层：

1. 将内核中的 ATA 驱动代码移除
2. 将内核中的 NE2000 驱动代码移除
3. 创建用户空间驱动进程
4. 修改 VFS 和块设备层通过 IPC 访问驱动
5. 修改网络层通过 IPC 访问驱动

## 📊 当前架构

```
┌─────────────────────────────────────┐
│         用户空间 (Ring 3)            │
├─────────────────────────────────────┤
│  应用程序                            │
│    ↕ 系统调用                       │
│  [未来] 驱动程序 (IPC 服务)         │
│    ↕ sys_request_io_port()          │
│    ↕ sys_register_irq_handler()     │
├═════════════════════════════════════┤
│         内核空间 (Ring 0)            │
├─────────────────────────────────────┤
│  微内核核心:                         │
│  - 进程管理                          │
│  - 内存管理                          │
│  - IPC                               │
│  - I/O 权限管理 ✅                  │
│  - IRQ 桥接 ✅                      │
│  - 系统调用                          │
│                                      │
│  [当前] 驱动程序 (内核中):           │
│  - ATA 驱动                          │
│  - NE2000 驱动                       │
│  - 键盘驱动                          │
└─────────────────────────────────────┘
```

## 🎯 目标架构

```
┌─────────────────────────────────────┐
│         用户空间 (Ring 3)            │
├─────────────────────────────────────┤
│  应用程序                            │
│    ↕ IPC                            │
│  驱动程序 (IPC 服务):               │
│  - ATA 驱动                          │
│  - NE2000 驱动                       │
│  - 键盘驱动                          │
│    ↕ sys_request_io_port()          │
│    ↕ sys_register_irq_handler()     │
├═════════════════════════════════════┤
│         内核空间 (Ring 0)            │
├─────────────────────────────────────┤
│  微内核核心（最小化）:               │
│  - 进程管理                          │
│  - 内存管理                          │
│  - IPC                               │
│  - I/O 权限管理 ✅                  │
│  - IRQ 桥接 ✅                      │
│  - 系统调用                          │
└─────────────────────────────────────┘
```

## 💡 实现建议

推荐先实现**方案 A（简化版）**，原因：
1. 快速展示微内核概念
2. 保持现有功能正常工作
3. 可以逐步迁移
4. 更容易调试

完成简化版后，可以选择是否继续实现完整版。

## 🔧 使用示例（计划中）

```c
// 用户空间 ATA 驱动示例
void ata_driver_main(void)
{
    // 请求 I/O 端口权限
    syscall2(SYS_REQUEST_IO_PORT, 0x1F0, 0x1F7);  // Primary ATA
    
    // 创建服务端口
    int port = syscall0(SYS_IPC_CREATE_NAMED_PORT, "ata_service");
    
    // 注册 IRQ 处理
    syscall2(SYS_REGISTER_IRQ_HANDLER, 14, port);
    
    // 服务循环
    while (1) {
        struct ipc_message msg;
        syscall2(SYS_IPC_RECV, port, &msg);
        
        if (msg.type == ATA_MSG_READ) {
            // 处理读请求
            ata_read_sector(...);
            // 发送响应
            syscall4(SYS_IPC_SEND, msg.sender_port, ...);
        }
    }
}
```

## ✅ 可以测试的功能

当前已经可以测试 I/O 权限和 IRQ 桥接：

```c
// 测试 I/O 权限
syscall2(SYS_REQUEST_IO_PORT, 0x3F8, 0x3FF);  // 串口
outb(0x3F8, 'A');  // 现在用户程序可以直接访问端口

// 测试 IRQ 桥接
int port = syscall0(SYS_IPC_CREATE_PORT);
syscall2(SYS_REGISTER_IRQ_HANDLER, 1, port);  // 注册键盘 IRQ
// 现在键盘中断会发送 IPC 消息到这个端口
```

