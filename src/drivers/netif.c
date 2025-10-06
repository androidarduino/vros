#include "netif.h"

#define MAX_NETIF 4

static struct netif netifs[MAX_NETIF];
static int netif_count = 0;

// 简单的字符串比较
static int strcmp_local(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

// 简单的字符串复制
static void strcpy_local(char *dest, const char *src)
{
    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// 初始化网络接口子系统
int netif_init(void)
{
    for (int i = 0; i < MAX_NETIF; i++)
    {
        netifs[i].name[0] = '\0';
        netifs[i].ops = 0;
        netifs[i].private_data = 0;
        netifs[i].stats.packets_sent = 0;
        netifs[i].stats.packets_received = 0;
        netifs[i].stats.bytes_sent = 0;
        netifs[i].stats.bytes_received = 0;
        netifs[i].stats.errors = 0;
    }
    netif_count = 0;
    return 0;
}

// 注册网络接口
int netif_register(const char *name, struct netif_ops *ops, void *private_data)
{
    if (netif_count >= MAX_NETIF)
        return -1;

    struct netif *netif = &netifs[netif_count++];
    strcpy_local(netif->name, name);
    netif->ops = ops;
    netif->private_data = private_data;

    // 获取 MAC 地址
    if (ops->get_mac)
        ops->get_mac(netif->mac_addr);

    return 0;
}

// 获取网络接口
struct netif *netif_get(const char *name)
{
    for (int i = 0; i < netif_count; i++)
    {
        if (strcmp_local(netifs[i].name, name) == 0)
            return &netifs[i];
    }
    return 0;
}

// 发送数据包
int netif_send(struct netif *netif, const uint8_t *data, uint16_t length)
{
    if (!netif || !netif->ops || !netif->ops->send)
        return -1;

    int result = netif->ops->send(data, length);
    if (result == 0)
    {
        netif->stats.packets_sent++;
        netif->stats.bytes_sent += length;
    }
    else
    {
        netif->stats.errors++;
    }

    return result;
}

// 接收数据包
int netif_receive(struct netif *netif, uint8_t *buffer, uint16_t max_length)
{
    if (!netif || !netif->ops || !netif->ops->receive)
        return -1;

    int length = netif->ops->receive(buffer, max_length);
    if (length > 0)
    {
        netif->stats.packets_received++;
        netif->stats.bytes_received += length;
    }

    return length;
}

// 获取所有网络接口
int netif_get_all(struct netif **list, int max_count)
{
    int count = netif_count < max_count ? netif_count : max_count;
    for (int i = 0; i < count; i++)
    {
        list[i] = &netifs[i];
    }
    return count;
}

