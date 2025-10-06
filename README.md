# VROS - Virtual Real-time Operating System

一个功能完整的 x86 **微内核**操作系统，从零开始用 C 和汇编语言编写。

## 🎯 项目概述

VROS 是一个教学性质的微内核操作系统，实现了现代操作系统的核心功能。**最重要的是**，它展示了真正的微内核架构，包括 **IPC（进程间通信）** 和 **用户空间驱动**。

## ⭐ 微内核特性（核心亮点！）

### IPC（进程间通信）
- ✅ **基于消息传递的IPC** - 微内核的核心
- ✅ **命名端口** - 服务发现机制（`"service_name"`）
- ✅ **阻塞/非阻塞接收** - `ipc_recv()` / `ipc_try_recv()`
- ✅ **消息队列** - 每端口16条消息缓冲
- ✅ **IPC统计** - 实时监控消息传递

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

### 用户空间驱动
- ✅ **驱动运行在用户空间（Ring 3）** - 不在内核中！
- ✅ **驱动崩溃不影响系统** - 隔离性
- ✅ **通过IPC通信** - 所有驱动<->应用通信通过消息传递
- ✅ **动态服务发现** - 客户端通过名字找到驱动
- ✅ **客户端注册机制** - 驱动管理多个客户端
- ✅ **事件广播** - 驱动向所有客户端分发事件

演示：模拟键盘驱动 + 两个客户端应用

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

### 文件系统
- ✅ **VFS（虚拟文件系统）** - 统一的文件系统接口
- ✅ **ramfs** - 根文件系统（内存文件系统，支持目录）
- ✅ **procfs** - 进程信息文件系统
  - `/proc/uptime` - 系统运行时间
  - `/proc/meminfo` - 内存使用统计
  - `/proc/tasks` - 进程列表
- ✅ **devfs** - 设备文件系统
  - `/dev/null` - 空设备
  - `/dev/zero` - 零设备
  - `/dev/random` - 随机数设备

### 用户模式
- ✅ **Ring 0/3 隔离** - 内核态/用户态分离
- ✅ **特权级切换** - 安全的模式转换
- ✅ **用户程序加载** - 自定义可执行格式
- ✅ **exec() 系统调用** - 加载并运行新程序

### Shell 命令行
```bash
help      - 显示帮助
clear     - 清屏
echo      - 回显文本
about     - 系统信息
mem       - 内存使用情况
heap      - 堆使用统计
ps        - 进程列表（带统计）
ls        - 列出文件/目录
cat       - 显示文件内容
mkdir     - 创建目录
rmdir     - 删除目录

# 测试命令
syscall   - 测试系统调用
devtest   - 测试设备文件
usertest  - 测试用户模式
forktest  - 测试 fork()
exectest  - 测试 exec()

# IPC 和驱动测试
ipctest   - 测试IPC通信
ipcstop   - 停止IPC测试
ipcinfo   - 显示IPC统计和端口信息
drvtest   - 测试用户空间驱动（微内核演示）
drvstop   - 停止驱动测试

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
│   │   └── ipc.c/h     # IPC实现
│   ├── mm/             # 内存管理
│   │   ├── pmm.c/h     # 物理内存管理
│   │   ├── paging.c/h  # 分页管理
│   │   └── kmalloc.c/h # 堆分配器
│   ├── fs/             # 文件系统
│   │   ├── vfs.c/h     # 虚拟文件系统
│   │   ├── ramfs.c/h   # RAM文件系统
│   │   ├── procfs.c/h  # /proc文件系统
│   │   ├── devfs.c/h   # /dev文件系统
│   │   └── exec.c/h    # 程序加载器
│   ├── drivers/        # 驱动
│   │   └── keyboard.c/h # 键盘驱动
│   ├── lib/            # 库函数
│   │   ├── shell.c/h   # Shell实现
│   │   ├── usermode.c/h # 用户模式支持
│   │   ├── user_prog.c  # 测试用户程序
│   │   ├── test_prog.c  # exec测试程序
│   │   ├── sched_test.c # 调度器测试
│   │   ├── ipc_test.c   # IPC测试
│   │   └── userspace_driver.c # 用户空间驱动演示
│   └── include/        # 头文件
├── build/              # 编译输出目录
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

系统启动后会显示 Shell 提示符：
```
VROS Shell >
```

尝试这些命令：
```bash
> help          # 查看所有命令
> about         # 系统信息
> ps            # 查看进程
> mem           # 查看内存
> ls /          # 列出根目录
> cat /proc/uptime   # 系统运行时间

# 测试IPC和微内核架构
> ipctest       # 启动IPC测试
> ipcinfo       # 查看IPC统计
> drvtest       # 启动用户空间驱动演示
> ps            # 查看驱动和客户端进程
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
3. **用户空间驱动** - 演示了如何将驱动移出内核
4. **隔离性** - 驱动崩溃不影响系统

### 关键文件说明

**IPC实现：**
- `src/kernel/ipc.c` - IPC核心实现（端口、消息队列）
- `src/include/ipc.h` - IPC API定义
- `src/lib/ipc_test.c` - IPC测试和演示

**用户空间驱动：**
- `src/lib/userspace_driver.c` - 键盘驱动演示
  - `userspace_keyboard_driver()` - 驱动任务
  - `keyboard_client_app()` - 客户端应用
  - 展示了驱动-应用通信模式

**进程管理：**
- `src/kernel/task.c` - 调度器、上下文切换
- `src/kernel/task_switch.s` - 汇编上下文切换

**系统调用：**
- `src/kernel/syscall.c` - 系统调用分发
- `src/kernel/syscall_asm.s` - INT 0x80 入口

## 🧪 测试场景

### 1. IPC通信测试
```bash
> ipctest       # 启动服务器和客户端
> ipcinfo       # 查看端口和消息统计
> ps            # 查看进程CPU使用
> ipcstop       # 停止测试
```

### 2. 用户空间驱动测试
```bash
> drvtest       # 启动驱动和两个客户端
> ps            # 查看：kbd_driver, kbd_client1, kbd_client2
> ipcinfo       # 查看驱动端口和消息广播
> drvstop       # 停止测试
```

### 3. 进程管理测试
```bash
> forktest      # 测试进程克隆
> exectest      # 测试程序执行
> ps            # 查看进程状态
```

### 4. 文件系统测试
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
│  • 客户端应用                                 │
├─────────────────────────────────────────────┤
│  服务层 (Ring 3) - 微内核特色！              │
│  • 用户空间驱动 (kbd_driver)                 │
│  • 文件系统服务器 (未来)                      │
│  • 网络栈 (未来)                             │
├─────────────────────────────────────────────┤
│  IPC 层 - 通信桥梁                           │
│  • 消息传递                                  │
│  • 命名端口                                  │
│  • 服务发现                                  │
├─────────────────────────────────────────────┤
│  微内核 (Ring 0) - 最小化                    │
│  • 进程调度                                  │
│  • 内存管理                                  │
│  • 基本I/O                                  │
│  • 中断处理                                  │
└─────────────────────────────────────────────┘
```

### IPC消息流

```
客户端                驱动               内核
  │                   │                  │
  │ 1. 查找端口       │                  │
  ├──────────────────>│                  │
  │                   │                  │
  │ 2. 注册           │                  │
  ├──────────────────>│                  │
  │                   │ 3. 添加到列表    │
  │                   │                  │
  │                   │ 4. 生成事件      │
  │                   │                  │
  │ 5. 广播事件       │                  │
  │<──────────────────┤                  │
  │                   │                  │
  │ 6. 处理事件       │                  │
```

## 📚 技术特点

### 1. 真正的微内核
- 内核代码最小化
- 服务在用户空间运行
- 通过IPC通信

### 2. 模块化设计
- VFS 抽象层
- 可插拔文件系统
- 独立的驱动进程

### 3. 安全性
- Ring 0/3 隔离
- 进程独立地址空间
- 驱动崩溃隔离

### 4. 可扩展性
- 命名端口机制
- 动态服务发现
- 易于添加新服务

## 🔮 未来扩展

### 短期目标
- [ ] 持久化存储（ATA驱动 + FAT文件系统）
- [ ] 更多POSIX系统调用（pipe, dup, signal）
- [ ] MLFQ调度器修复和完善

### 中期目标
- [ ] 多核支持（SMP）
- [ ] 网络栈（以太网 + TCP/IP）
- [ ] 更多用户空间驱动

### 长期目标
- [ ] 图形模式（VESA/VBE）
- [ ] 简单GUI系统
- [ ] 移植应用程序

## 🤝 贡献

这是一个教学项目，欢迎学习和改进！

## 📜 许可证

MIT License

## 🙏 致谢

感谢所有操作系统开发资源和社区的贡献者！

---

**VROS** - 从零构建的微内核操作系统 🚀
