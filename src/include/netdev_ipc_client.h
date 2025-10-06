/*
 * 网络设备 IPC 客户端接口
 */

#ifndef NETDEV_IPC_CLIENT_H
#define NETDEV_IPC_CLIENT_H

#include <stdint.h>

// 初始化网络设备 IPC 客户端
int netdev_ipc_client_init(void);

// 检查驱动是否可用
int netdev_ipc_driver_available(void);

// 发送数据包
int netdev_ipc_send(const uint8_t *data, uint32_t length);

// 接收数据包
int netdev_ipc_recv(uint8_t *buffer, uint32_t max_length);

// 获取 MAC 地址
int netdev_ipc_get_mac(uint8_t *mac);

#endif // NETDEV_IPC_CLIENT_H
