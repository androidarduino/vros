# IPC (进程间通信) 实现指南

## 概述

VROS现在支持基于消息传递的IPC（Inter-Process Communication）系统，这是微内核架构的核心功能。

## 核心概念

### 端口（Port）
- 进程通信的端点
- 每个端口由创建它的进程拥有
- 最多支持32个端口
- 每个端口有16个消息的队列

### 消息（Message）
```c
struct ipc_message {
    uint32_t sender_pid;     // 发送进程的PID
    uint32_t type;           // 消息类型（用户定义）
    uint32_t size;           // 数据大小
    char data[256];          // 消息数据（最大256字节）
};
```

## 系统调用API

### 1. 创建端口
```c
int ipc_create_port(void);
```
- 返回值：成功返回port_id（0-31），失败返回-1
- 创建的端口由当前进程拥有

### 2. 销毁端口
```c
int ipc_destroy_port(uint32_t port_id);
```
- 只能销毁自己拥有的端口
- 会唤醒等待在该端口上的进程

### 3. 发送消息
```c
int ipc_send(uint32_t dest_port, uint32_t type, const void *data, uint32_t size);
```
- dest_port：目标端口ID
- type：消息类型（用户自定义）
- data：消息数据指针
- size：数据大小（最大256字节）
- 返回值：成功返回0，失败返回-1

### 4. 接收消息（阻塞）
```c
int ipc_recv(uint32_t port_id, struct ipc_message *msg);
```
- port_id：自己的端口ID
- msg：接收消息的缓冲区
- 如果队列为空，进程会被阻塞直到有消息到达
- 返回值：成功返回0，失败返回-1

## 使用示例

### 服务器进程
```c
void server_task(void)
{
    // 创建端口
    int port = ipc_create_port();
    
    // 接收消息
    struct ipc_message msg;
    while (1) {
        if (ipc_recv(port, &msg) == 0) {
            // 处理消息
            printf("Received from PID %d: %s\n", 
                   msg.sender_pid, msg.data);
        }
    }
}
```

### 客户端进程
```c
void client_task(void)
{
    uint32_t server_port = 0;  // 服务器的端口ID
    
    char message[] = "Hello Server!";
    ipc_send(server_port, 1, message, sizeof(message));
}
```

## 测试命令

在VROS Shell中，你可以使用以下命令测试IPC：

```
> ipctest    # 启动IPC测试（创建服务器和客户端任务）
> ps         # 查看任务状态
> ipcstop    # 停止IPC测试
```

## 测试流程

1. `ipctest` 创建两个任务：
   - **ipc_server**: 创建端口0，接收3条消息
   - **ipc_client**: 向端口0发送3条消息

2. 客户端发送消息：
   - "Hello from client!"
   - "Second message"
   - "Goodbye!"

3. 服务器接收并处理这些消息

4. 使用 `ps` 查看任务状态和CPU ticks

5. 使用 `ipcstop` 停止测试任务

## 特性

### 1. 阻塞接收
- `ipc_recv()` 是阻塞调用
- 如果队列为空，进程自动进入BLOCKED状态
- 当消息到达时，进程自动唤醒

### 2. 消息队列
- 每个端口有16个消息的缓冲区
- 使用循环队列实现
- FIFO（先进先出）顺序

### 3. 进程隔离
- 只有端口所有者可以接收消息
- 任何进程都可以向任何端口发送消息
- 端口销毁时会自动清理资源

## 实现细节

### 数据结构
```c
struct ipc_port {
    uint32_t port_id;
    uint32_t owner_pid;
    int in_use;
    
    // 消息队列
    struct ipc_message queue[16];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;
    
    // 等待的任务
    struct task *waiting_task;
};
```

### 全局端口表
- 32个端口槽位
- 在 `ipc_init()` 中初始化
- 内核启动时调用

## 限制

1. **端口数量**：最多32个端口
2. **消息大小**：每条消息最大256字节
3. **队列深度**：每个端口最多16条消息
4. **阻塞方式**：只支持阻塞接收（将来可以添加非阻塞模式）

## 未来扩展

1. **非阻塞接收**：`ipc_try_recv()`
2. **广播消息**：向多个端口发送相同消息
3. **消息优先级**：紧急消息优先处理
4. **共享内存IPC**：用于大数据传输
5. **信号量和互斥锁**：基于IPC实现同步原语

## 微内核应用

有了IPC，你可以：

1. **驱动程序用户化**：将设备驱动移到用户空间
2. **文件系统服务**：文件系统作为用户空间服务器
3. **网络栈**：网络协议栈在用户空间运行
4. **窗口系统**：GUI作为独立的服务进程

## 系统调用号

```c
#define SYS_IPC_CREATE_PORT  8
#define SYS_IPC_DESTROY_PORT 9
#define SYS_IPC_SEND         10
#define SYS_IPC_RECV         11
```

## 示例：简单的请求-响应模式

```c
// 服务器
void echo_server(void) {
    int port = ipc_create_port();
    struct ipc_message msg;
    
    while (1) {
        ipc_recv(port, &msg);
        // Echo back
        ipc_send(msg.sender_pid, msg.type, msg.data, msg.size);
    }
}

// 客户端
void echo_client(uint32_t server_port) {
    int my_port = ipc_create_port();
    char request[] = "Echo this!";
    
    // 发送请求
    ipc_send(server_port, 1, request, sizeof(request));
    
    // 等待响应
    struct ipc_message response;
    ipc_recv(my_port, &response);
    
    ipc_destroy_port(my_port);
}
```

## 总结

IPC系统为VROS提供了进程间通信的基础设施，是实现真正微内核架构的关键。通过消息传递，系统服务可以在用户空间运行，提高了系统的模块化、安全性和可靠性。

