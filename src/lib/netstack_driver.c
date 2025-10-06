/*
 * 用户空间网络协议栈
 *
 * 这是一个运行在 Ring 3 的网络协议栈实现
 * 通过 IPC 与 NE2000 驱动和应用程序通信
 *
 * 架构：
 * 应用 <--IPC--> 网络协议栈 <--IPC--> NE2000驱动 <--I/O--> 硬件
 */

#include <stdint.h>

// 系统调用
#define SYS_YIELD 4
#define SYS_IPC_CREATE_NAMED_PORT 12
#define SYS_IPC_RECV 11
#define SYS_IPC_TRY_RECV 14

static inline void syscall_yield(void)
{
    __asm__ volatile("int $0x80" : : "a"(SYS_YIELD));
}

static inline int syscall_ipc_create_named_port(const char *name)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_CREATE_NAMED_PORT), "b"(name));
    return ret;
}

// IPC 消息结构（必须在使用前定义）
struct ipc_message_user
{
    uint32_t sender_pid;
    uint32_t sender_port;
    uint32_t type;
    uint32_t size;
    char data[256];
};

static inline int syscall_ipc_recv(uint32_t port, struct ipc_message_user *msg)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_RECV), "b"(port), "c"(msg));
    return ret;
}

static inline int syscall_ipc_try_recv(uint32_t port, struct ipc_message_user *msg)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_TRY_RECV), "b"(port), "c"(msg));
    return ret;
}

// ============= 网络协议栈数据结构 =============

// 网络字节序转换
#define htons(x) ((uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF)))
#define ntohs(x) htons(x)

// MAC 地址
typedef struct
{
    uint8_t addr[6];
} __attribute__((packed)) mac_addr_t;

// IP 地址
typedef uint32_t ip_addr_t;

// 以太网帧类型
#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IP 0x0800

// 以太网头部
typedef struct
{
    mac_addr_t dest;
    mac_addr_t src;
    uint16_t type;
} __attribute__((packed)) eth_header_t;

// ARP 包
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY 2

typedef struct
{
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_addr_len;
    uint8_t proto_addr_len;
    uint16_t opcode;
    mac_addr_t sender_mac;
    ip_addr_t sender_ip;
    mac_addr_t target_mac;
    ip_addr_t target_ip;
} __attribute__((packed)) arp_packet_t;

// IP 协议号
#define IP_PROTO_ICMP 1

// IP 头部
typedef struct
{
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    ip_addr_t src;
    ip_addr_t dest;
} __attribute__((packed)) ip_header_t;

// ICMP 类型
#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_TYPE_ECHO_REQUEST 8

// ICMP 头部
typedef struct
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

// ============= 网络配置 =============

static ip_addr_t my_ip = 0x0A00020F; // 10.0.2.15 (QEMU 默认)
static mac_addr_t my_mac = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};

// ARP 缓存
#define ARP_CACHE_SIZE 16
typedef struct
{
    ip_addr_t ip;
    mac_addr_t mac;
    int valid;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];

// ============= 辅助函数 =============

static void mac_copy(mac_addr_t *dest, const mac_addr_t *src)
{
    for (int i = 0; i < 6; i++)
        dest->addr[i] = src->addr[i];
}

static int ip_equal(ip_addr_t a, ip_addr_t b)
{
    return a == b;
}

static uint16_t ip_checksum(const void *data, uint32_t len)
{
    const uint16_t *ptr = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1)
    {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1)
        sum += *(const uint8_t *)ptr;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ~sum;
}

// ============= 协议处理 =============

// 处理 ARP 包
static void process_arp(const arp_packet_t *arp)
{
    uint16_t opcode = ntohs(arp->opcode);

    // 更新 ARP 缓存
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if (!arp_cache[i].valid || ip_equal(arp_cache[i].ip, arp->sender_ip))
        {
            arp_cache[i].ip = arp->sender_ip;
            mac_copy(&arp_cache[i].mac, &arp->sender_mac);
            arp_cache[i].valid = 1;
            break;
        }
    }

    // 如果是 ARP 请求且目标是我们，发送 ARP 回复
    if (opcode == ARP_OP_REQUEST && ip_equal(arp->target_ip, my_ip))
    {
        // 这里应该通过 IPC 发送 ARP 回复给 NE2000 驱动
        // 由于需要实现完整的 IPC 通信，暂时留空
        (void)opcode; // 避免警告
    }
}

// 处理 ICMP 包 (ping)
static void process_icmp(const icmp_header_t *icmp, const uint8_t *data, uint32_t len, ip_addr_t src_ip)
{
    (void)data;
    (void)len;
    (void)src_ip;

    // 验证校验和
    uint8_t temp[512];
    if (sizeof(icmp_header_t) + len > 512)
        return;

    // 如果是 ICMP Echo 请求，应该发送回复
    if (icmp->type == ICMP_TYPE_ECHO_REQUEST)
    {
        // 这里应该通过 IPC 发送 ICMP 回复
        // 暂时留空，展示架构
    }
}

// 处理 IP 包
static void process_ip(const ip_header_t *ip, const uint8_t *payload, uint32_t payload_len)
{
    // 检查目标 IP 是否是我们
    if (!ip_equal(ip->dest, my_ip))
        return;

    // 根据协议分发
    switch (ip->protocol)
    {
    case IP_PROTO_ICMP:
        if (payload_len >= sizeof(icmp_header_t))
            process_icmp((const icmp_header_t *)payload,
                         payload + sizeof(icmp_header_t),
                         payload_len - sizeof(icmp_header_t),
                         ip->src);
        break;
    }
}

// 处理以太网帧
static void process_ethernet_frame(const uint8_t *frame, uint32_t len)
{
    if (len < sizeof(eth_header_t))
        return;

    const eth_header_t *eth = (const eth_header_t *)frame;
    const uint8_t *payload = frame + sizeof(eth_header_t);
    uint32_t payload_len = len - sizeof(eth_header_t);

    uint16_t type = ntohs(eth->type);

    switch (type)
    {
    case ETH_TYPE_ARP:
        if (payload_len >= sizeof(arp_packet_t))
            process_arp((const arp_packet_t *)payload);
        break;

    case ETH_TYPE_IP:
        if (payload_len >= sizeof(ip_header_t))
            process_ip((const ip_header_t *)payload,
                       payload + sizeof(ip_header_t),
                       payload_len - sizeof(ip_header_t));
        break;
    }
}

// ============= 主循环 =============

// 调试输出函数
#define SYS_WRITE 1
static void debug_print(const char *msg)
{
    int len = 0;
    while (msg[len]) len++;
    __asm__ volatile(
        "int $0x80"
        : : "a"(SYS_WRITE), "b"(1), "c"(msg), "d"(len));
}

void netstack_driver_main(void)
{
    debug_print("[netstack] Starting...\n");
    
    // 初始化 ARP 缓存
    debug_print("[netstack] Initializing ARP cache...\n");
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        arp_cache[i].valid = 0;

    debug_print("[netstack] Creating IPC port...\n");
    // 创建命名 IPC 端口（供应用程序连接）
    int port = syscall_ipc_create_named_port("net.stack");
    
    if (port < 0)
    {
        debug_print("[netstack] Port creation FAILED!\n");
        // 端口创建失败，让出 CPU
        while (1)
            syscall_yield();
    }
    
    debug_print("[netstack] Port created successfully\n");
    debug_print("[netstack] Entering main loop\n");

    // 主循环：接收网络数据包和应用请求
    while (1)
    {
        
        struct ipc_message_user msg;

        // 使用非阻塞接收，避免阻塞整个系统
        int recv_result = syscall_ipc_try_recv(port, &msg);
        if (recv_result == 0)
        {
            debug_print("[netstack] Message received\n");
            // 判断消息类型
            if (msg.type == 1) // 网络数据包
            {
                // 处理接收到的以太网帧
                process_ethernet_frame((const uint8_t *)msg.data, msg.size);
            }
            else if (msg.type == 2) // 应用程序请求
            {
                // 处理应用程序的网络请求（socket, send, recv等）
                // 这需要实现完整的 socket API
            }
        }

        // 让出 CPU，让其他任务运行
        syscall_yield();
    }
}
