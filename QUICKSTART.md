# VROS 快速开始指南

## 🚀 5分钟快速上手

### 1. 编译系统
```bash
cd /home/george/vros
make
```

### 2. 运行系统
```bash
make run
```

### 3. 测试命令
在 QEMU 窗口中尝试：
```bash
help          # 查看所有命令
ls /          # 列出文件
cat /hello.txt # 读取文件
mem           # 查看内存
ps            # 查看进程
forktest      # 测试fork
exectest      # 测试exec
```

---

## 📝 常用命令速查

### 系统管理
| 命令 | 说明 | 示例 |
|------|------|------|
| `help` | 显示帮助 | `help` |
| `about` | 系统信息 | `about` |
| `clear` | 清屏 | `clear` |

### 内存管理
| 命令 | 说明 | 示例 |
|------|------|------|
| `mem` | 内存统计 | `mem` |
| `heap` | 堆统计 | `heap` |
| `page` | 页表测试 | `page` |
| `malloc` | 测试分配 | `malloc` |

### 进程管理
| 命令 | 说明 | 示例 |
|------|------|------|
| `ps` | 进程信息 | `ps` |
| `forktest` | 测试fork | `forktest` |
| `exectest` | 测试exec | `exectest` |

### 文件系统
| 命令 | 说明 | 示例 |
|------|------|------|
| `ls` | 列出文件 | `ls /` |
| `cat` | 读取文件 | `cat /hello.txt` |

### 测试工具
| 命令 | 说明 | 示例 |
|------|------|------|
| `syscall` | 测试系统调用 | `syscall` |
| `devtest` | 测试设备 | `devtest` |
| `usertest` | 测试用户模式 | `usertest` |

---

## 🔧 开发工作流

### 修改代码后重新编译
```bash
make clean    # 清理
make          # 重新编译
make run      # 运行测试
```

### 创建 ISO 镜像
```bash
make image
qemu-system-x86_64 -cdrom vros.iso
```

### 调试
```bash
# 终端1: 启动QEMU等待GDB
qemu-system-x86_64 -kernel kernel.bin -s -S

# 终端2: 启动GDB
gdb kernel.bin
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

---

## 📂 项目文件导航

### 需要修改的常见文件

**添加新系统调用**:
```
src/include/syscall.h    # 添加系统调用号
src/kernel/syscall.c     # 实现处理函数
```

**添加新 Shell 命令**:
```
src/lib/shell.c          # 添加命令函数和解析
```

**修改内存管理**:
```
src/mm/pmm.c            # 物理内存
src/mm/paging.c         # 虚拟内存
src/mm/kmalloc.c        # 堆分配
```

**添加新设备驱动**:
```
src/drivers/            # 创建新驱动文件
src/include/            # 添加头文件
```

---

## 🐛 常见问题

### Q: 编译失败？
```bash
# 确保安装了依赖
sudo apt install build-essential

# 清理后重新编译
make clean && make
```

### Q: QEMU 不显示输出？
- 检查 VGA 缓冲区代码 (0xB8000)
- 确认中断已启用 (sti)
- 查看是否触发三重错误

### Q: 系统崩溃重启？
- 检查栈是否设置正确
- 验证 GDT/IDT 是否初始化
- 使用 GDB 调试

### Q: 修改后不生效？
```bash
make clean  # 清理旧的编译文件
make        # 重新完整编译
```

---

## 🎯 学习路径

### 初学者 (已完成 ✅)
- [x] 理解启动过程
- [x] 实现基本的 I/O
- [x] 建立中断系统
- [x] 实现内存管理

### 中级 (当前)
- [x] 实现多任务
- [x] 添加系统调用
- [x] 实现文件系统
- [ ] 完善进程管理

### 高级 (下一步)
- [ ] 实现 IPC
- [ ] 添加网络支持
- [ ] 实现用户权限
- [ ] 多核支持

---

## 📚 有用的资源

### 在线资源
- [OSDev Wiki](https://wiki.osdev.org/) - 最全面的OS开发资源
- [Intel 手册](https://www.intel.com/sdm) - x86架构详解
- [Linux 0.11](https://github.com/yuan/linux-0.11) - 学习早期Linux

### 本项目文档
- `README.md` - 完整项目文档
- `STATUS.md` - 项目状态和计划
- `QUICKSTART.md` - 本文档

### 目录说明
```
src/boot/     - 从这里开始启动
src/kernel/   - 核心功能实现
src/mm/       - 内存管理重点
src/fs/       - 文件系统实现
```

---

## ⚡ 性能提示

### 加快编译速度
```bash
make -j$(nproc)  # 并行编译
```

### QEMU 优化选项
```bash
# 使用 KVM 加速 (Linux)
qemu-system-x86_64 -kernel kernel.bin -enable-kvm

# 增加内存
qemu-system-x86_64 -kernel kernel.bin -m 256M
```

---

## 🎓 推荐练习

1. **添加新命令**: 在 shell.c 中添加 `date` 命令
2. **实现新系统调用**: 添加 `sys_sleep(milliseconds)`
3. **改进调度器**: 实现优先级调度
4. **扩展文件系统**: 在 ramfs 中添加目录支持
5. **新设备驱动**: 实现串口驱动

---

**祝你开发愉快！** 🚀

如果遇到问题，查看 `README.md` 或 `STATUS.md` 获取更多信息。

