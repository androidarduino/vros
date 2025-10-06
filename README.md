# VROS - Virtual Real-time Operating System

一个功能完整的 x86 **微内核**操作系统，从零开始用 C 和汇编语言编写。

## 🎯 项目概述

VROS 是一个教学性质的微内核操作系统，实现了现代操作系统的核心功能。**最重要的是**，它展示了真正的微内核架构，包括 **IPC（进程间通信）**、**用户空间驱动** 和 **持久化存储**。

## ⭐ 微内核特性（核心亮点！）

### IPC（进程间通信）
- ✅ **基于消息传递的IPC** - 微内核的核心
- ✅ **命名端口** - 服务发现机制（`"service_name"`）
- ✅ **阻塞/非阻塞接收** - `ipc_recv()` / `ipc_try_recv()`
- ✅ **消息队列** - 每端口16条消息缓冲
- ✅ **IPC统计** - 实时监控消息传递
- ✅ **发送者端口追踪** - 支持请求-响应模式

系统调用：
```c
int ipc_create_port()                           // 创建端口
int ipc_create_named_port(name)                 // 创建命名端口
int ipc_find_port(name)                         // 查找端口
int ipc_send(port, type, data, size)            // 发送消息
int ipc_recv(port, msg)                         // 接收消息（阻塞）
int ipc_try_recv(port, msg)                     // 接收消息（非阻塞）
int ipc_destroy_port(port)                      // 销毁端口
```

### 用户空间驱动（真正的微内核！）
- ✅ **驱动运行在用户空间（Ring 3）** - 不在内核中！
- ✅ **驱动崩溃不影响系统** - 完全隔离
- ✅ **通过IPC通信** - 所有驱动<->应用通信通过消息传递
- ✅ **I/O端口权限系统** - IOPB in TSS，用户空间I/O访问
- ✅ **IRQ到IPC桥接** - 中断通过IPC传递到用户空间
- ✅ **自动启动** - 驱动随系统启动

**已实现的用户空间驱动和服务：**
1. **ATA/IDE磁盘驱动** (`ata_driver`) - 完全在用户空间 ✅
2. **NE2000网络驱动** (`ne2000_driver`) - 完全在用户空间 ✅
3. **网络协议栈** (`netstack`) - TCP/IP协议栈运行在用户空间！✅
4. **键盘驱动演示** (`kbd_driver`) - 演示驱动-客户端通信

### 持久化存储
- ✅ **ATA/IDE驱动** - PIO模式，支持LBA寻址
- ✅ **块设备抽象层** - 统一的块设备接口
- ✅ **VRFS文件系统** - VR Operating System File System
  - 超级块 + inode表 + 数据块
  - 位图管理（inode和数据块）
  - 目录支持
- ✅ **挂载系统** - 支持多文件系统挂载
- ✅ **持久化文件** - 重启后数据保留

### 网络支持（真正的微内核！）
- ✅ **NE2000网卡驱动** - ISA网卡支持（用户空间）
- ✅ **MAC地址读取** - 从PROM或PAR寄存器
- ✅ **数据包发送/接收** - 基础网络I/O
- ✅ **网络协议栈** - **完全在Ring 3运行**
  - ARP 协议和缓存管理
  - IP 数据包处理
  - ICMP (ping) 支持
  - 通过 IPC 与驱动和应用通信
- ✅ **微内核架构** - 网络栈崩溃不影响系统

## ✨ 完整特性列表

### 系统基础
- ✅ **Multiboot 兼容** - 可通过 GRUB 启动
- ✅ **GDT/IDT** - 完整的段描述符和中断描述符表
- ✅ **中断处理** - 异常处理、硬件中断（PIC）
- ✅ **VGA 文本显示** - 80x25 文本模式，硬件光标，屏幕滚动
- ✅ **键盘驱动** - 扫描码处理，Shift/CapsLock 支持

### 内存管理
- ✅ **物理内存管理器（PMM）** - 位图分配算法
- ✅ **虚拟内存管理器（VMM）** - 二级页表，按需分页
- ✅ **内核堆分配器** - kmalloc/kfree，首次适配算法
- ✅ **页表复制** - 支持进程独立地址空间

### 进程管理
- ✅ **多任务调度** - 轮转调度算法（Round-Robin）
- ✅ **上下文切换** - 完整的寄存器状态保存/恢复
- ✅ **进程树** - 父子关系、兄弟进程
- ✅ **进程状态** - READY, RUNNING, BLOCKED, ZOMBIE, SLEEPING
- ✅ **进程统计** - CPU时间、上下文切换计数
- ✅ **fork/exec** - 完整的进程创建和程序执行

### 系统调用（INT 0x80）

**进程管理：**
```c
sys_exit(status)              // 退出进程
sys_getpid()                  // 获取进程ID
sys_yield()                   // 让出CPU
sys_fork()                    // 复制进程
sys_waitpid(pid, status)      // 等待子进程退出
sys_execve(path, argv, envp)  // 加载并执行新程序
```

**I/O：**
```c
sys_write(fd, buf, count)     // 写入数据
sys_read(fd, buf, count)      // 读取数据
```

**IPC：**
```c
sys_ipc_create_port()                    // 创建端口
sys_ipc_create_named_port(name)          // 创建命名端口
sys_ipc_find_port(name)                  // 查找端口
sys_ipc_send(port, type, data, size)     // 发送消息
sys_ipc_recv(port, msg)                  // 接收消息（阻塞）
sys_ipc_try_recv(port, msg)              // 接收消息（非阻塞）
sys_ipc_destroy_port(port)               // 销毁端口
```

**用户空间驱动支持：**
```c
sys_request_io_port(start, end)          // 请求I/O端口访问权限
sys_register_irq_handler(irq, port)      // 注册IRQ处理器（通过IPC）
```

### 文件系统
- ✅ **VFS（虚拟文件系统）** - 统一的文件系统接口
- ✅ **ramfs** - 根文件系统（内存文件系统，支持目录）
- ✅ **VRFS** - 持久化文件系统（基于ATA磁盘）
- ✅ **procfs** - 进程信息文件系统
  - `/proc/uptime` - 系统运行时间
  - `/proc/meminfo` - 内存使用统计
  - `/proc/tasks` - 进程列表
- ✅ **devfs** - 设备文件系统
  - `/dev/null` - 空设备
  - `/dev/zero` - 零设备
  - `/dev/random` - 随机数设备
- ✅ **路径解析** - 支持绝对路径、相对路径、`.` 和 `..`
- ✅ **挂载点** - `/mnt` 自动挂载持久化存储

### 用户模式
- ✅ **Ring 0/3 隔离** - 内核态/用户态分离
- ✅ **特权级切换** - 安全的模式转换
- ✅ **用户程序加载** - 自定义可执行格式
- ✅ **exec() 系统调用** - 加载并运行新程序

### Shell 命令行
```bash
# 基础命令
help      - 显示帮助
clear     - 清屏
echo      - 回显文本
about     - 系统信息
mem       - 内存使用情况
heap      - 堆使用统计
ps        - 进程列表（带统计）

# 文件系统
ls [path]     - 列出文件/目录
cat <file>    - 显示文件内容
mkdir <dir>   - 创建目录
rmdir <dir>   - 删除目录
cd <dir>      - 切换目录
rm <file>     - 删除文件
touch <file>  - 创建空文件
write <file>  - 写入文本到文件

# 持久化存储
lsblk         - 列出块设备
mkfs <dev>    - 格式化设备（VRFS）
mount <dev> <path> - 挂载文件系统
umount <path> - 卸载文件系统

# 网络
ifconfig      - 显示网络接口信息
nettest       - 测试网络功能

# 测试命令
syscall   - 测试系统调用
devtest   - 测试设备文件
usertest  - 测试用户模式
forktest  - 测试 fork()
exectest  - 测试 exec()
atatest   - 测试ATA读写

# IPC 和驱动测试
ipctest   - 测试IPC通信
ipcstop   - 停止IPC测试
ipcinfo   - 显示IPC统计和端口信息
drvtest   - 测试用户空间驱动（微内核演示）
drvstop   - 停止驱动测试
blktest   - 测试块设备IPC
net2ktest - 测试NE2000网络IPC

# 调度器测试
schedtest - 测试调度器
schedstop - 停止调度测试
```

## 📂 目录结构

```
vros/
├── src/
│   ├── boot/           # 引导代码
│   │   └── boot.s      # Multiboot 引导
│   ├── kernel/         # 核心内核
│   │   ├── kernel.c    # 内核主函数
│   │   ├── idt.c/h     # 中断描述符表
│   │   ├── isr.s       # 中断服务例程（汇编）
│   │   ├── isr_handler.c # ISR C 处理函数
│   │   ├── pic.c/h     # 可编程中断控制器
│   │   ├── task.c/h    # 任务管理
│   │   ├── task_switch.s # 上下文切换（汇编）
│   │   ├── syscall.c/h # 系统调用
│   │   ├── syscall_asm.s # 系统调用入口（汇编）
│   │   ├── ipc.c/h     # IPC实现
│   │   ├── ioport.c/h  # I/O端口权限管理
│   │   └── irq_bridge.c/h # IRQ到IPC桥接
│   ├── mm/             # 内存管理
│   │   ├── pmm.c/h     # 物理内存管理
│   │   ├── paging.c/h  # 分页管理
│   │   └── kmalloc.c/h # 堆分配器
│   ├── fs/             # 文件系统
│   │   ├── vfs.c/h     # 虚拟文件系统
│   │   ├── ramfs.c/h   # RAM文件系统
│   │   ├── procfs.c/h  # /proc文件系统
│   │   ├── devfs.c/h   # /dev文件系统
│   │   ├── vrfs.c/h    # VR文件系统（持久化）
│   │   ├── mount.c/h   # 挂载管理
│   │   └── exec.c/h    # 程序加载器
│   ├── drivers/        # 驱动（内核空间）
│   │   ├── keyboard.c/h # 键盘驱动
│   │   ├── ata.c/h     # ATA驱动（内核层）
│   │   ├── blkdev.c/h  # 块设备抽象
│   │   ├── ata_blk.c/h # ATA块设备包装
│   │   ├── ne2000.c/h  # NE2000驱动（内核层）
│   │   ├── netif.c/h   # 网络接口
│   │   ├── blkdev_ipc_client.c/h # 块设备IPC客户端
│   │   └── netdev_ipc_client.c/h # 网络设备IPC客户端
│   ├── lib/            # 库函数和用户空间驱动
│   │   ├── shell.c/h   # Shell实现
│   │   ├── usermode.c/h # 用户模式支持
│   │   ├── user_prog.c  # 测试用户程序
│   │   ├── test_prog.c  # exec测试程序
│   │   ├── sched_test.c # 调度器测试
│   │   ├── ipc_test.c   # IPC测试
│   │   ├── userspace_driver.c # 键盘驱动演示
│   │   ├── ata_driver.c # ATA用户空间驱动
│   │   └── ne2000_driver.c # NE2000用户空间驱动
│   └── include/        # 头文件
│       ├── blkdev_ipc.h # 块设备IPC协议
│       └── netdev_ipc.h # 网络设备IPC协议
├── build/              # 编译输出目录
├── disk.img            # 虚拟磁盘镜像
├── Makefile            # 构建脚本
├── linker.ld           # 链接器脚本
├── README.md           # 本文件
└── IPC_GUIDE.md        # IPC详细指南
```

## 🚀 快速开始

### 环境要求

```bash
# Ubuntu/Debian
sudo apt-get install build-essential gcc nasm qemu-system-x86

# Arch Linux
sudo pacman -S base-devel gcc nasm qemu-system-x86
```

### 编译和运行

```bash
# 编译
make

# 在 QEMU 中运行
make run

# 清理
make clean
```

### 启动后

系统启动后会自动：
1. ✅ 启动用户空间 ATA 驱动
2. ✅ 启动用户空间 NE2000 网络驱动
3. ✅ 自动挂载持久化文件系统到 `/mnt`
4. ✅ 显示 Shell 提示符

```
VROS Shell >
```

尝试这些命令：
```bash
> help          # 查看所有命令
> about         # 系统信息
> ps            # 查看进程（会看到 ata_driver 和 ne2000_driver）
> mem           # 查看内存
> ls /          # 列出根目录
> ls /mnt       # 列出持久化存储

# 测试持久化存储
> cd /mnt
> touch hello.txt
> write hello.txt
Hello, VROS!    # 输入文本后按回车结束
> cat hello.txt
> ls /mnt

# 重启系统后，文件仍然存在！

# 测试IPC和微内核架构
> ipcinfo       # 查看驱动的IPC端口
> blktest       # 测试块设备IPC（与ata_driver通信）
> net2ktest     # 测试网络IPC（与ne2000_driver通信）
> drvtest       # 启动键盘驱动演示
```

## 🎓 学习资源

### 微内核架构

**什么是微内核？**
- 内核只包含最小功能：IPC、调度、内存管理
- 驱动、文件系统等都在用户空间
- 通过IPC通信，模块化程度高
- 更安全、更可靠、更易维护

**VROS 的微内核实现：**
1. **最小化内核** - 只包含核心功能
2. **IPC机制** - 消息传递、命名端口、服务发现
3. **用户空间驱动** - ATA、NE2000等驱动完全在Ring 3
4. **I/O权限管理** - IOPB in TSS，安全的用户空间I/O
5. **IRQ桥接** - 中断通过IPC传递到用户空间
6. **隔离性** - 驱动崩溃不影响系统

### 关键文件说明

**IPC实现：**
- `src/kernel/ipc.c` - IPC核心实现（端口、消息队列）
- `src/include/ipc.h` - IPC API定义
- `src/lib/ipc_test.c` - IPC测试和演示

**用户空间驱动和服务：**
- `src/lib/ata_driver.c` - ATA磁盘驱动（用户空间）
- `src/lib/ne2000_driver.c` - NE2000网络驱动（用户空间）
- `src/lib/netstack_driver.c` - **网络协议栈（用户空间）**
- `src/lib/userspace_driver.c` - 键盘驱动演示
- `src/include/blkdev_ipc.h` - 块设备IPC协议
- `src/include/netdev_ipc.h` - 网络设备IPC协议

**IPC客户端层：**
- `src/drivers/blkdev_ipc_client.c` - 内核通过IPC访问块设备
- `src/drivers/netdev_ipc_client.c` - 内核通过IPC访问网络设备

**进程管理：**
- `src/kernel/task.c` - 调度器、上下文切换
- `src/kernel/task_switch.s` - 汇编上下文切换

**系统调用：**
- `src/kernel/syscall.c` - 系统调用分发
- `src/kernel/syscall_asm.s` - INT 0x80 入口

**持久化存储：**
- `src/drivers/ata.c` - ATA硬件驱动（内核层）
- `src/fs/vrfs.c` - VRFS文件系统实现
- `src/fs/mount.c` - 挂载点管理

## 🧪 测试场景

### 1. 用户空间驱动测试
```bash
> ps            # 查看 ata_driver 和 ne2000_driver 进程
> ipcinfo       # 查看驱动端口："blkdev.ata" 和 "netdev.ne2000"
> blktest       # 测试块设备IPC通信
> net2ktest     # 测试网络设备IPC通信
```

### 2. 持久化存储测试
```bash
# 在 /mnt 创建文件
> cd /mnt
> touch test.txt
> write test.txt
Hello World!
> cat test.txt
> ls /mnt

# 重启QEMU，文件应该还在
> ls /mnt
> cat test.txt

# 测试目录
> mkdir /mnt/docs
> cd docs
> touch readme.md
> ls
```

### 3. IPC通信测试
```bash
> ipctest       # 启动服务器和客户端
> ipcinfo       # 查看端口和消息统计
> ps            # 查看进程CPU使用
> ipcstop       # 停止测试
```

### 4. 键盘驱动演示测试
```bash
> drvtest       # 启动驱动和两个客户端
> ps            # 查看：kbd_driver, kbd_client1, kbd_client2
> ipcinfo       # 查看驱动端口和消息广播
> drvstop       # 停止测试
```

### 5. 进程管理测试
```bash
> forktest      # 测试进程克隆
> exectest      # 测试程序执行
> ps            # 查看进程状态
```

### 6. 文件系统测试
```bash
> ls /          # 列出根目录
> cat /proc/tasks       # 查看进程列表
> cat /proc/meminfo     # 查看内存信息
> mkdir /test   # 创建目录
> ls /          # 验证创建
```

## 🏗️ 架构设计

### 微内核层次结构

```
┌─────────────────────────────────────────────┐
│  应用层 (Ring 3)                             │
│  • Shell                                     │
│  • 用户程序                                   │
│  • 测试程序                                   │
├─────────────────────────────────────────────┤
│  服务层 (Ring 3) - 微内核核心！              │
│  • ATA驱动 (ata_driver)                      │
│  • NE2000驱动 (ne2000_driver)                │
│  • 网络协议栈 (netstack) ← 在用户空间！      │
│  • 键盘驱动演示 (kbd_driver)                 │
│  • 未来：文件系统服务器                       │
├─────────────────────────────────────────────┤
│  IPC 层 - 通信桥梁                           │
│  • 消息传递（sender_port追踪）               │
│  • 命名端口（"blkdev.ata", "netdev.ne2000"） │
│  • 服务发现                                  │
│  • IRQ桥接（中断→IPC）                       │
├─────────────────────────────────────────────┤
│  微内核 (Ring 0) - 最小化                    │
│  • 进程调度                                  │
│  • 内存管理                                  │
│  • I/O权限管理（IOPB）                       │
│  • 中断处理                                  │
│  • IPC实现                                   │
└─────────────────────────────────────────────┘
```

### 块设备IPC消息流

```
应用/内核           用户空间驱动          硬件
  │                     │                  │
  │ 1. 查找端口         │                  │
  │  ipc_find_port()    │                  │
  ├────────────────────>│                  │
  │                     │                  │
  │ 2. 发送读请求       │                  │
  │  (sector, buffer)   │                  │
  ├────────────────────>│                  │
  │                     │ 3. I/O操作       │
  │                     ├─────────────────>│
  │                     │ 4. 数据传输      │
  │                     │<─────────────────┤
  │ 5. 响应             │                  │
  │  (status, bytes)    │                  │
  │<────────────────────┤                  │
```

### 驱动自动启动流程

```
内核启动
  │
  ├─> 初始化 IPC 系统
  │
  ├─> 初始化 I/O 端口权限系统
  │
  ├─> 初始化 IRQ 桥接
  │
  ├─> 创建 ata_driver 任务
  │     └─> 请求 I/O 端口 (0x1F0-0x1F7)
  │     └─> 创建命名端口 "blkdev.ata"
  │     └─> 进入消息循环
  │
  ├─> 创建 ne2000_driver 任务
  │     └─> 创建命名端口 "netdev.ne2000"
  │     └─> 进入消息循环
  │
  ├─> 自动挂载 /mnt (使用 blkdev.ata)
  │
  └─> 启动 Shell
```

## 📚 技术特点

### 1. 真正的微内核
- 内核代码最小化（只有核心功能）
- **驱动在用户空间运行**（Ring 3）
- 通过IPC通信（消息传递）
- 驱动崩溃不会导致系统崩溃

### 2. 模块化设计
- VFS 抽象层（支持多文件系统）
- 可插拔文件系统（ramfs, procfs, devfs, vrfs）
- 独立的驱动进程（ata_driver, ne2000_driver）
- 标准化的IPC协议

### 3. 安全性
- Ring 0/3 隔离（内核/用户）
- 进程独立地址空间
- I/O端口权限控制（IOPB）
- 驱动崩溃隔离

### 4. 可扩展性
- 命名端口机制（服务发现）
- 动态服务注册
- 标准化的IPC协议
- 易于添加新服务/驱动

## 🎉 最近更新（2025-10）

### ✅ 已完成：用户空间网络协议栈

**网络协议栈完全在用户空间运行！**

1. **用户空间网络协议栈** (`netstack`)
   - 运行在 Ring 3
   - ARP 协议和缓存
   - IP 数据包处理
   - ICMP (ping) 支持
   - 通过 IPC 通信

2. **完整的网络架构**
   ```
   应用 <--IPC--> 网络协议栈 <--IPC--> NE2000驱动 <--I/O--> 硬件
   (Ring 3)       (Ring 3)              (Ring 3)
   ```

3. **微内核优势**
   - 网络栈崩溃不影响系统
   - 可以独立升级协议栈
   - 更好的安全隔离

### ✅ 已完成：用户空间驱动架构
1. **I/O端口权限系统**
   - IOPB (I/O Permission Bitmap) in TSS
   - `sys_request_io_port()` 系统调用
   - 用户空间安全访问硬件端口

2. **IRQ到IPC桥接**
   - 硬件中断通过IPC传递
   - `sys_register_irq_handler()` 系统调用
   - 用户空间中断处理

3. **用户空间ATA驱动**
   - 完全在Ring 3运行
   - 通过IPC提供块设备服务
   - 命名端口：`"blkdev.ata"`
   - 支持读/写/刷新操作

4. **用户空间NE2000驱动**
   - 完全在Ring 3运行
   - 通过IPC提供网络服务
   - 命名端口：`"netdev.ne2000"`
   - 支持MAC地址读取、数据包发送/接收

5. **IPC客户端层**
   - `blkdev_ipc_client` - 内核访问块设备
   - `netdev_ipc_client` - 内核访问网络设备
   - VFS通过IPC与用户空间驱动通信

6. **驱动自动启动**
   - 系统启动时自动创建驱动任务
   - 无需手动启动

### ✅ 已完成：持久化存储
1. **ATA/IDE驱动**
   - PIO模式读写
   - LBA寻址
   - 28位LBA支持

2. **块设备抽象层**
   - 统一的块设备接口
   - 支持多种块设备

3. **VRFS文件系统**
   - 超级块 + inode表
   - 位图管理
   - 目录和文件支持
   - 重启后数据保留

4. **自动挂载**
   - `/mnt` 自动挂载到 `hda`
   - 首次启动自动格式化

5. **Shell命令**
   - `lsblk` - 列出块设备
   - `mkfs` - 格式化设备
   - `mount/umount` - 挂载管理
   - `touch/write/rm` - 文件操作
   - `cd` - 目录切换

## 🔮 未来扩展

### 短期目标
- [ ] 网络协议栈（ARP, IP, ICMP, TCP/UDP）
- [ ] 更多POSIX系统调用（pipe, dup, signal）
- [ ] 动态链接和ELF加载器

### 中期目标
- [ ] 多核支持（SMP）
- [ ] 完整的TCP/IP实现
- [ ] 文件系统服务器（用户空间）
- [ ] 更多用户空间驱动（USB, VirtIO）

### 长期目标
- [ ] 图形模式（VESA/VBE）
- [ ] 简单GUI系统
- [ ] 移植应用程序（shell工具、编辑器）
- [ ] 动态模块加载

## 📊 项目统计

- **代码行数**: ~10,000+ 行 C/汇编
- **文件系统**: 4种（ramfs, procfs, devfs, vrfs）
- **系统调用**: 17个
- **驱动程序**: 3个用户空间驱动
- **Shell命令**: 35+ 个

## 🤝 贡献

这是一个教学项目，欢迎学习和改进！

## 📜 许可证

MIT License

## 🙏 致谢

感谢所有操作系统开发资源和社区的贡献者！

特别感谢：
- OSDev Wiki - 丰富的操作系统开发资源
- QEMU项目 - 优秀的模拟器
- 所有微内核架构的先驱者

---

**VROS** - 真正的微内核操作系统，驱动在用户空间！🚀
