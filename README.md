# VROS - Virtual Real-time Operating System

一个功能完整的 x86 **微内核**操作系统，从零开始用 C 和汇编语言编写。

## 🎯 项目概述

VROS 是一个教学性质的微内核操作系统，实现了现代操作系统的核心功能。**最重要的是**，它展示了真正的微内核架构，包括 **IPC（进程间通信）**、**用户空间驱动** 和 **网络协议栈**。

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
- ✅ **配置化管理** - `driver_config.c` 集中管理驱动启用/禁用

**已实现的用户空间驱动和服务：**
1. **ATA/IDE磁盘驱动** (`ata_driver`) - 完全在用户空间 ✅
2. **NE2000网络驱动** (`ne2000_driver`) - 完全在用户空间 ✅
3. **网络协议栈** (`netstack`) - ARP/IP/ICMP协议栈运行在用户空间！✅
4. **键盘驱动演示** (`kbd_driver`) - 演示驱动-客户端通信

### 持久化存储
- ✅ **ATA/IDE驱动** - PIO模式，支持LBA寻址
- ✅ **块设备抽象层** - 统一的块设备接口
- ✅ **VRFS文件系统** - VR Operating System File System
  - 超级块 + inode表 + 数据块
  - 位图管理（inode和数据块）
  - 目录支持（`mkdir`/`rmdir`）
- ✅ **挂载系统** - 支持多文件系统挂载
- ✅ **持久化文件** - 重启后数据保留
- ✅ **自动挂载** - 启动时自动挂载 `/mnt`

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
- ✅ **清洁实现** - 无调试输出，生产级代码质量

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
- ✅ **sleep/wake** - 进程休眠和唤醒机制

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
sys_read(fd, buf, count)      // 读取数据
sys_write(fd, buf, count)     // 写入数据
```

**IPC：**
```c
sys_ipc_create_port()                        // 创建IPC端口
sys_ipc_create_named_port(name)              // 创建命名端口
sys_ipc_find_port(name)                      // 查找命名端口
sys_ipc_destroy_port(port)                   // 销毁端口
sys_ipc_send(port, type, data, size)         // 发送消息
sys_ipc_recv(port, msg)                      // 接收消息（阻塞）
sys_ipc_try_recv(port, msg)                  // 接收消息（非阻塞）
```

**用户空间驱动支持：**
```c
sys_request_io_port(start, end)              // 请求I/O端口权限
sys_register_irq_handler(irq, port)          // 注册IRQ到IPC端口
```

### 虚拟文件系统（VFS）
- ✅ **VFS层** - 统一的文件系统接口
- ✅ **ramfs** - 内存文件系统，支持目录
- ✅ **procfs** - 进程信息文件系统（`/proc`）
  - `/proc/uptime` - 系统运行时间
  - `/proc/meminfo` - 内存信息
  - `/proc/tasks` - 进程列表
- ✅ **devfs** - 设备文件系统（`/dev`）
  - `/dev/null` - 黑洞设备
  - `/dev/zero` - 零设备
  - `/dev/random` - 随机数设备
- ✅ **VRFS** - 持久化文件系统（磁盘）
  - 支持文件和目录
  - 自动挂载到 `/mnt`
- ✅ **挂载系统** - 支持多挂载点

### Shell 命令

**基础命令：**
- `help` - 显示所有可用命令
- `clear` - 清屏
- `mem` - 显示内存使用情况
- `ps` - 显示进程列表

**文件系统：**
- `ls [path]` - 列出目录内容
- `cd <path>` - 切换目录
- `cat <file>` - 显示文件内容
- `touch <file>` - 创建空文件
- `write <file> <text>` - 写入文件
- `rm <file>` - 删除文件
- `mkdir <dir>` - 创建目录
- `rmdir <dir>` - 删除目录

**存储管理：**
- `lsblk` - 列出块设备
- `mkfs <device>` - 格式化文件系统
- `mount [device] [path]` - 挂载或查看挂载点
- `umount <path>` - 卸载文件系统

**进程管理：**
- `syscall` - 测试系统调用
- `usertest` - 用户模式测试
- `forktest` - fork系统调用测试
- `exectest` - exec系统调用测试

**IPC测试：**
- `ipctest` - IPC消息传递测试
- `ipcstop` - 停止IPC测试任务
- `ipcinfo` - 显示IPC统计信息

**驱动测试：**
- `drvtest` - 用户空间驱动测试
- `drvstop` - 停止驱动测试
- `atadrv` - 启动ATA驱动
- `blktest` - 块设备IPC测试
- `atatest` - ATA读写测试

**网络：**
- `ifconfig` - 显示网络配置
- `nettest` - 网络设备测试
- `netdrv` - 启动NE2000驱动
- `net2ktest` - NE2000驱动测试
- `netstacktest` - 网络协议栈测试
- `arp` - 显示ARP缓存
- `ping <ip>` - 发送ICMP Echo请求

## 🚀 快速开始

### 环境要求
- GCC (32-bit support)
- NASM/AS
- QEMU
- GNU Make

### 编译和运行
```bash
# 编译
make

# 运行（QEMU用户模式网络）
make run

# 运行（TAP网络）
sudo ./setup-tap.sh
make run-tap

# 清理
make clean
```

### 首次使用
```bash
# 1. 启动系统（按任意键进入shell）
make run

# 2. 查看帮助
help

# 3. 查看进程
ps

# 4. 测试文件系统
ls /
ls /proc
ls /dev

# 5. 创建持久化文件
cd /mnt
touch hello.txt
write hello.txt "Hello, VROS!"
cat hello.txt

# 6. 测试网络（查看配置）
ifconfig

# 7. 查看IPC统计
ipcinfo
```

## 📁 项目结构

```
vros/
├── src/
│   ├── boot/          # 启动代码
│   ├── kernel/        # 内核核心
│   │   ├── kernel.c   # 主内核代码
│   │   ├── idt.c      # 中断描述符表
│   │   ├── isr.c      # 中断服务程序
│   │   ├── syscall.c  # 系统调用处理
│   │   ├── task.c     # 进程管理
│   │   ├── ipc.c      # IPC实现
│   │   └── driver_config.c  # 驱动配置
│   ├── mm/            # 内存管理
│   │   ├── pmm.c      # 物理内存管理
│   │   ├── paging.c   # 虚拟内存管理
│   │   └── kmalloc.c  # 内核堆分配器
│   ├── fs/            # 文件系统
│   │   ├── vfs.c      # 虚拟文件系统
│   │   ├── ramfs.c    # 内存文件系统
│   │   ├── procfs.c   # 进程文件系统
│   │   ├── devfs.c    # 设备文件系统
│   │   ├── vrfs.c     # VRFS持久化文件系统
│   │   └── mount.c    # 挂载管理
│   ├── drivers/       # 内核驱动（最小化）
│   │   ├── keyboard.c # 键盘驱动
│   │   ├── ata.c      # ATA初始化
│   │   └── ne2000.c   # NE2000初始化
│   ├── lib/           # 用户空间驱动和程序
│   │   ├── shell.c    # 命令行shell
│   │   ├── ata_driver.c      # 用户空间ATA驱动
│   │   ├── ne2000_driver.c   # 用户空间NE2000驱动
│   │   └── netstack_driver.c # 用户空间网络协议栈
│   └── include/       # 头文件
│       ├── ipc.h
│       ├── socket_ipc.h
│       ├── driver_config.h
│       └── ...
├── Makefile
├── linker.ld
└── README.md
```

## 🏗️ 架构设计

### 微内核架构

```
┌─────────────────────────────────────────────────┐
│            用户空间（Ring 3）                    │
├─────────────────────────────────────────────────┤
│  应用程序          Shell          用户程序       │
│     │               │               │           │
│     └───────────────┴───────────────┘           │
│                     │ IPC                       │
│     ┌───────────────┴───────────────┐           │
│     │     网络协议栈 (netstack)      │           │
│     │   ARP / IP / ICMP / TCP / UDP │           │
│     └───────────────┬───────────────┘           │
│                     │ IPC                       │
│     ┌───────────────┴───────────────┐           │
│     │  ATA驱动     │  NE2000驱动    │           │
│     │ (ata_driver) │(ne2000_driver) │           │
│     └──────┬───────┴───────┬────────┘           │
│            │ IOPB          │ IOPB               │
├────────────┼───────────────┼────────────────────┤
│     内核空间（Ring 0）- 最小化                   │
├────────────┼───────────────┼────────────────────┤
│    [调度器] [IPC] [内存] [VFS] [初始化]         │
│            │               │                    │
│     ┌──────┴───────┐ ┌─────┴──────┐            │
│     │ ATA硬件初始化│ │NE2000初始化│            │
│     └──────┬───────┘ └─────┬──────┘            │
└────────────┼───────────────┼────────────────────┘
             │               │
        ┌────┴────┐     ┌────┴─────┐
        │ ATA硬件 │     │NE2000硬件│
        └─────────┘     └──────────┘
```

### IPC通信流程

```
应用程序                网络协议栈              NE2000驱动
   │                       │                       │
   ├─ ipc_send() ─────────>│                       │
   │   (type=2, data)      │                       │
   │                       ├─ 处理请求              │
   │                       ├─ 构造以太网帧          │
   │                       ├─ ipc_send() ─────────>│
   │                       │   (type=2, frame)     │
   │                       │                       ├─ 发送到硬件
   │                       │                       ├─ outw(dataport)
   │                       │                       │
   │                       │                       ├─ 轮询接收
   │                       │                       ├─ inb(ISR)
   │                       │                       ├─ 读取数据包
   │                       │<─ ipc_send() ─────────┤
   │                       │   (type=1, packet)    │
   │                       ├─ 处理数据包            │
   │<─ ipc_send() ─────────┤                       │
   │   (response)          │                       │
```

## 🔬 技术亮点

### 1. 真正的微内核
- 驱动运行在用户空间，不在内核
- 内核只提供最基础的服务（调度、IPC、内存、VFS）
- 驱动崩溃不会导致系统崩溃

### 2. 完整的IPC系统
- 消息传递机制
- 命名端口服务发现
- 阻塞/非阻塞接收
- 请求-响应模式支持

### 3. 用户空间I/O
- I/O Permission Bitmap (IOPB) in TSS
- 用户进程可直接访问授权的I/O端口
- 保持隔离性和安全性

### 4. 网络协议栈在用户空间
- 完整的ARP/IP/ICMP实现
- 运行在Ring 3
- 通过IPC与驱动通信
- 可以独立崩溃和重启

### 5. 持久化存储
- 真实的磁盘文件系统（VRFS）
- 支持文件和目录
- 重启后数据保留

### 6. 清洁代码质量
- 移除所有调试输出
- 生产级代码质量
- 良好的模块化设计

## 🛠️ 开发工具链

- **编译器**: GCC 32-bit
- **汇编器**: GNU AS
- **链接器**: GNU LD
- **模拟器**: QEMU
- **构建工具**: GNU Make
- **调试**: GDB (可选)

## 📊 系统统计

- **代码行数**: ~15,000+ 行 C/Assembly
- **系统调用**: 16个
- **支持的文件系统**: 4个（ramfs, procfs, devfs, vrfs）
- **用户空间驱动**: 3个（ATA, NE2000, netstack）
- **Shell命令**: 30+个
- **内核大小**: ~300KB

## 🎓 学习价值

这个项目展示了以下重要概念：

1. **微内核架构** - 真正的微内核设计，而不是宏内核
2. **IPC机制** - 进程间通信的实现
3. **用户空间驱动** - 驱动在用户空间运行
4. **网络协议栈** - TCP/IP协议栈实现
5. **虚拟内存** - 分页、页表、地址转换
6. **进程调度** - 多任务和上下文切换
7. **文件系统** - VFS和具体文件系统实现
8. **持久化存储** - 磁盘驱动和文件系统
9. **x86保护模式** - Ring 0/3, GDT, IDT, TSS
10. **硬件编程** - 直接操作硬件寄存器

## 🔧 故障排除

### QEMU网络问题
如果网络不工作：
1. 确保使用了正确的QEMU网络配置
2. 对于ICMP ping，使用`-net user,restrict=no`
3. 或使用TAP网络：`sudo ./setup-tap.sh && make run-tap`

### 文件系统问题
如果文件系统无法挂载：
1. 运行 `mkfs hda` 格式化磁盘
2. 运行 `mount hda /mnt` 手动挂载
3. 检查 `lsblk` 确认设备存在

### 驱动问题
如果驱动未启动：
1. 检查 `ps` 命令查看驱动进程
2. 检查 `src/kernel/driver_config.c` 中的驱动配置
3. 查看启动时的输出信息

## 📚 参考资料

- [OSDev Wiki](https://wiki.osdev.org/)
- [Intel® 64 and IA-32 Architectures Software Developer's Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [xv6: a simple, Unix-like teaching operating system](https://pdos.csail.mit.edu/6.828/2020/xv6.html)
- [Minix 3](https://www.minix3.org/) - 微内核设计参考

## 📝 许可证

此项目仅用于教育目的。

## 🙏 致谢

感谢所有操作系统开发社区的贡献者，特别是OSDev Wiki的维护者们。

---

**VROS - 从零开始构建真正的微内核操作系统！** 🚀
