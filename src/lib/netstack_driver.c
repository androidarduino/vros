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
#define SYS_IPC_SEND 10
#define SYS_IPC_RECV 11
#define SYS_IPC_CREATE_NAMED_PORT 12
#define SYS_IPC_FIND_PORT 13
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
#define htonl(x) ((uint32_t)((((x) & 0xFF) << 24) | (((x) & 0xFF00) << 8) | (((x) & 0xFF0000) >> 8) | (((x) >> 24) & 0xFF)))
#define ntohl(x) htonl(x)

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

// 前置声明
#define SYS_WRITE 1
static void debug_print(const char *msg);
static void send_frame_to_driver(const uint8_t *frame, uint32_t len);

// 处理 ARP 包
static void process_arp(const arp_packet_t *arp)
{
    uint16_t opcode = ntohs(arp->opcode);
    ip_addr_t sender_ip = ntohl(arp->sender_ip);
    ip_addr_t target_ip = ntohl(arp->target_ip);

    // 更新 ARP 缓存
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if (!arp_cache[i].valid || ip_equal(arp_cache[i].ip, sender_ip))
        {
            arp_cache[i].ip = sender_ip;
            mac_copy(&arp_cache[i].mac, &arp->sender_mac);
            arp_cache[i].valid = 1;
            break;
        }
    }

    // 如果是 ARP 请求且目标是我们，发送 ARP 回复
    if (opcode == ARP_OP_REQUEST && ip_equal(target_ip, my_ip))
    {
        // 构造 ARP 响应帧
        uint8_t reply_frame[42]; // 14 (eth) + 28 (ARP)

        // 以太网头部
        mac_copy((mac_addr_t *)&reply_frame[0], &arp->sender_mac); // 目标 MAC
        mac_copy((mac_addr_t *)&reply_frame[6], &my_mac);          // 源 MAC
        reply_frame[12] = 0x08;                                    // EtherType: ARP
        reply_frame[13] = 0x06;

        // ARP 包
        arp_packet_t *reply = (arp_packet_t *)&reply_frame[14];
        reply->hw_type = htons(1);         // Ethernet
        reply->proto_type = htons(0x0800); // IPv4
        reply->hw_addr_len = 6;
        reply->proto_addr_len = 4;
        reply->opcode = htons(ARP_OP_REPLY);            // ARP Reply
        mac_copy(&reply->sender_mac, &my_mac);          // 我的 MAC
        reply->sender_ip = htonl(my_ip);                // 我的 IP（转为网络字节序）
        mac_copy(&reply->target_mac, &arp->sender_mac); // 请求者的 MAC
        reply->target_ip = arp->sender_ip;              // 请求者的 IP（保持网络字节序）

        // 发送给 NE2000 驱动
        send_frame_to_driver(reply_frame, 42);
    }
}

// 系统调用前置声明
static inline int syscall_ipc_send(int dest_port, uint32_t type, const void *data, uint32_t size);
static inline int syscall_ipc_find_port(const char *name);

// 处理 ICMP 包 (ping)
static void process_icmp(const icmp_header_t *icmp, const uint8_t *data, uint32_t len, ip_addr_t src_ip)
{
    (void)data;
    (void)len;

    // 如果是 ICMP Echo Reply（ping 回复）
    if (icmp->type == ICMP_TYPE_ECHO_REPLY)
    {
        // TODO: 通知 ping 命令任务（需要维护一个等待列表）
        // 目前不做处理
    }
    // 如果是 ICMP Echo 请求（从网络接收到的）
    else if (icmp->type == ICMP_TYPE_ECHO_REQUEST)
    {

        // 从 ARP 缓存中查找源 IP 对应的 MAC 地址
        mac_addr_t *src_mac = 0;
        for (int i = 0; i < ARP_CACHE_SIZE; i++)
        {
            if (arp_cache[i].valid && ip_equal(arp_cache[i].ip, src_ip))
            {
                src_mac = &arp_cache[i].mac;
                break;
            }
        }

        if (!src_mac)
            return;

        // 计算完整数据包大小
        uint32_t icmp_data_len = len;
        uint32_t icmp_total_len = sizeof(icmp_header_t) + icmp_data_len;
        uint32_t ip_total_len = sizeof(ip_header_t) + icmp_total_len;
        uint32_t frame_len = sizeof(eth_header_t) + ip_total_len;

        // 构造回复帧（最大1500字节）
        uint8_t reply_frame[1500];
        if (frame_len > 1500)
            return;

        // 以太网头部
        eth_header_t *eth = (eth_header_t *)reply_frame;
        mac_copy(&eth->dest, src_mac);  // 目标 MAC
        mac_copy(&eth->src, &my_mac);   // 源 MAC
        eth->type = htons(ETH_TYPE_IP); // EtherType: IPv4

        // IP 头部
        ip_header_t *ip_reply = (ip_header_t *)(reply_frame + sizeof(eth_header_t));
        ip_reply->version_ihl = 0x45; // IPv4, 20字节头部
        ip_reply->tos = 0;
        ip_reply->total_length = htons(ip_total_len);
        ip_reply->id = 0;
        ip_reply->flags_fragment = 0;
        ip_reply->ttl = 64;
        ip_reply->protocol = IP_PROTO_ICMP;
        ip_reply->checksum = 0;         // 稍后计算
        ip_reply->src = htonl(my_ip);   // 我的 IP
        ip_reply->dest = htonl(src_ip); // 目标 IP

        // 计算 IP 校验和（暂时跳过，很多实现都接受校验和为0）
        ip_reply->checksum = 0;

        // ICMP 头部
        icmp_header_t *icmp_reply = (icmp_header_t *)(reply_frame + sizeof(eth_header_t) + sizeof(ip_header_t));
        icmp_reply->type = ICMP_TYPE_ECHO_REPLY; // Echo Reply
        icmp_reply->code = 0;
        icmp_reply->checksum = 0;              // 稍后计算
        icmp_reply->id = icmp->id;             // 原样返回
        icmp_reply->sequence = icmp->sequence; // 原样返回

        // 复制 ICMP 数据（原样返回）
        uint8_t *icmp_data_dst = reply_frame + sizeof(eth_header_t) + sizeof(ip_header_t) + sizeof(icmp_header_t);
        for (uint32_t i = 0; i < icmp_data_len; i++)
        {
            icmp_data_dst[i] = data[i];
        }

        // 计算 ICMP 校验和
        uint32_t sum = 0;
        uint16_t *ptr = (uint16_t *)icmp_reply;
        for (uint32_t i = 0; i < icmp_total_len / 2; i++)
        {
            sum += ntohs(ptr[i]);
        }
        if (icmp_total_len & 1)
        {
            sum += ((uint8_t *)icmp_reply)[icmp_total_len - 1] << 8;
        }
        while (sum >> 16)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        icmp_reply->checksum = htons(~sum);

        // 发送回复
        send_frame_to_driver(reply_frame, frame_len);
    }
}

// 处理 IP 包
static void process_ip(const ip_header_t *ip, const uint8_t *payload, uint32_t payload_len)
{
    // 检查目标 IP 是否是我们（注意字节序转换）
    ip_addr_t dest_ip = ntohl(ip->dest);
    ip_addr_t src_ip = ntohl(ip->src);

    if (!ip_equal(dest_ip, my_ip))
        return;

    // 根据协议分发
    switch (ip->protocol)
    {
    case IP_PROTO_ICMP:
        if (payload_len >= sizeof(icmp_header_t))
            process_icmp((const icmp_header_t *)payload,
                         payload + sizeof(icmp_header_t),
                         payload_len - sizeof(icmp_header_t),
                         src_ip);
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
        {
            const ip_header_t *ip = (const ip_header_t *)payload;

            // 检查是否是 ICMP Echo Request（来自本地 ping 命令）
            if (ip->protocol == IP_PROTO_ICMP && payload_len >= sizeof(ip_header_t) + sizeof(icmp_header_t))
            {
                const icmp_header_t *icmp = (const icmp_header_t *)(payload + sizeof(ip_header_t));
                if (icmp->type == ICMP_TYPE_ECHO_REQUEST)
                {
                    // 这是来自本地 ping 命令的请求，转发到网络
                    send_frame_to_driver(frame, len);
                    return; // 已处理，直接返回
                }
            }

            // 处理接收到的 IP 数据包
            process_ip(ip, payload + sizeof(ip_header_t), payload_len - sizeof(ip_header_t));
        }
        break;
    }
}

// ============= 主循环 =============

// 调试输出函数（实现）
static void debug_print(const char *msg)
{
    int len = 0;
    while (msg[len])
        len++;
    __asm__ volatile(
        "int $0x80"
        : : "a"(SYS_WRITE), "b"(1), "c"(msg), "d"(len));
}

// 系统调用：发送 IPC 消息（实现）
static inline int syscall_ipc_send(int dest_port, uint32_t type, const void *data, uint32_t size)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_SEND), "b"(dest_port), "c"(type), "d"(data), "S"(size));
    return ret;
}

// 系统调用：查找命名端口（实现）
static inline int syscall_ipc_find_port(const char *name)
{
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_FIND_PORT), "b"(name));
    return ret;
}

// 全局变量：NE2000 驱动端口（用于发送）
static int ne2000_port = -1;

// 发送以太网帧到 NE2000 驱动（实现）
static void send_frame_to_driver(const uint8_t *frame, uint32_t len)
{
    if (ne2000_port < 0)
    {
        // 查找 NE2000 驱动端口
        ne2000_port = syscall_ipc_find_port("netdev.ne2000");
        if (ne2000_port < 0)
            return;
    }

    // 通过 IPC 发送数据包给 NE2000 驱动
    // 使用 type = 2 表示发送请求（NETDEV_OP_SEND_PACKET）
    syscall_ipc_send(ne2000_port, 2, frame, len);
}

void netstack_driver_main(void)
{
    // 初始化 ARP 缓存
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        arp_cache[i].valid = 0;

    // 创建命名 IPC 端口（供应用程序连接）
    int port = syscall_ipc_create_named_port("net.stack");

    if (port < 0)
    {
        // 端口创建失败，让出 CPU
        while (1)
            syscall_yield();
    }

    // 主循环：接收网络数据包和应用请求
    while (1)
    {
        struct ipc_message_user msg;

        // 使用非阻塞接收，避免阻塞整个系统
        int recv_result = syscall_ipc_try_recv(port, &msg);
        if (recv_result == 0)
        {
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
