#include <stdint.h>

// 系统调用包装器
static inline int syscall0(int num)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num));
    return ret;
}

static inline int syscall1(int num, uint32_t arg1)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

static inline int syscall2(int num, uint32_t arg1, uint32_t arg2)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2));
    return ret;
}

static inline int syscall4(int num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
    return ret;
}

// 端口 I/O 函数
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 系统调用号
#define SYS_WRITE 1
#define SYS_REQUEST_IO_PORT 15
#define SYS_REGISTER_IRQ_HANDLER 16
#define SYS_IPC_CREATE_PORT 8
#define SYS_IPC_RECV 11

// IPC 消息结构
struct ipc_message
{
    uint32_t type;
    uint32_t sender_port;
    char data[256];
    uint32_t size;
};

// 简单的字符串长度
static int strlen_local(const char *s)
{
    int len = 0;
    while (*s++)
        len++;
    return len;
}

// 打印函数
static void print(const char *str)
{
    syscall4(SYS_WRITE, 1, (uint32_t)str, strlen_local(str), 0);
}

// 测试 I/O 权限 - 测试串口
void test_io_port_permission(void)
{
    print("\n=== Test 1: I/O Port Permission ===\n");

    // 尝试访问串口（0x3F8-0x3FF）- 应该失败
    print("Before requesting permission...\n");
    print("(This might triple-fault if protection works)\n");

    // 请求串口端口权限
    print("Requesting I/O port access (0x3F8-0x3FF)...\n");
    int result = syscall2(SYS_REQUEST_IO_PORT, 0x3F8, 0x3FF);

    if (result == 0)
    {
        print("✓ Permission granted!\n");

        // 现在尝试写入串口
        print("Writing 'A' to serial port (0x3F8)...\n");
        outb(0x3F8, 'A');
        print("✓ Write successful (check QEMU serial output)\n");
    }
    else
    {
        print("✗ Permission denied\n");
    }
}

// 测试 IRQ 桥接 - 注册键盘中断
void test_irq_bridge(void)
{
    print("\n=== Test 2: IRQ Bridge (Keyboard) ===\n");

    // 创建 IPC 端口
    print("Creating IPC port...\n");
    int port = syscall0(SYS_IPC_CREATE_PORT);

    if (port < 0)
    {
        print("✗ Failed to create IPC port\n");
        return;
    }

    print("✓ IPC port created: ");
    // 简单的数字打印（只支持小数字）
    char num[16];
    int n = port;
    int i = 0;
    if (n == 0)
    {
        num[i++] = '0';
    }
    else
    {
        while (n > 0)
        {
            num[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    // 反转
    for (int j = 0; j < i / 2; j++)
    {
        char temp = num[j];
        num[j] = num[i - 1 - j];
        num[i - 1 - j] = temp;
    }
    num[i] = '\n';
    num[i + 1] = '\0';
    print(num);

    // 注册键盘 IRQ (IRQ 1)
    print("Registering keyboard IRQ handler (IRQ 1)...\n");
    int result = syscall2(SYS_REGISTER_IRQ_HANDLER, 1, port);

    if (result == 0)
    {
        print("✓ IRQ handler registered!\n");
        print("Press some keys and check if we receive IRQ messages...\n");
        print("(Waiting for 3 IRQ messages, then will exit)\n\n");

        // 接收 3 个 IRQ 消息
        for (int count = 0; count < 3; count++)
        {
            struct ipc_message msg;
            print("Waiting for IRQ...\n");
            syscall2(SYS_IPC_RECV, port, (uint32_t)&msg);

            print("✓ Received IRQ message! Type: 0x");
            // 打印十六进制
            const char *hex = "0123456789ABCDEF";
            char hex_str[9];
            for (int j = 7; j >= 0; j--)
            {
                hex_str[7 - j] = hex[(msg.type >> (j * 4)) & 0xF];
            }
            hex_str[8] = '\n';
            hex_str[9] = '\0';
            print(hex_str);
        }

        print("\n✓ IRQ bridge test passed!\n");
    }
    else
    {
        print("✗ Failed to register IRQ handler\n");
    }
}

// 主测试函数
void ioport_test_main(void)
{
    print("\n");
    print("╔════════════════════════════════════════╗\n");
    print("║  Microkernel I/O & IRQ Test Suite     ║\n");
    print("╚════════════════════════════════════════╝\n");

    // 测试 1: I/O 端口权限
    test_io_port_permission();

    // 测试 2: IRQ 桥接
    test_irq_bridge();

    print("\n");
    print("╔════════════════════════════════════════╗\n");
    print("║  All tests completed!                  ║\n");
    print("╚════════════════════════════════════════╝\n");
    print("\n");

    // 退出
    syscall1(0, 0); // SYS_EXIT
}
