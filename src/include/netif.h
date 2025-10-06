#ifndef NETIF_H
#define NETIF_H

#include <stdint.h>

// 网络接口统计
struct netif_stats
{
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t errors;
};

// 网络接口操作
struct netif_ops
{
    int (*send)(const uint8_t *data, uint16_t length);
    int (*receive)(uint8_t *buffer, uint16_t max_length);
    void (*get_mac)(uint8_t *mac);
};

// 网络接口
struct netif
{
    char name[16];
    uint8_t mac_addr[6];
    struct netif_ops *ops;
    struct netif_stats stats;
    void *private_data;
};

// 函数声明
int netif_init(void);
int netif_register(const char *name, struct netif_ops *ops, void *private_data);
struct netif *netif_get(const char *name);
int netif_send(struct netif *netif, const uint8_t *data, uint16_t length);
int netif_receive(struct netif *netif, uint8_t *buffer, uint16_t max_length);
int netif_get_all(struct netif **list, int max_count);

#endif // NETIF_H

