/*
 * 网络设备 IPC 协议
 *
 * 用于内核和用户空间网络驱动之间的通信
 */

#ifndef NETDEV_IPC_H
#define NETDEV_IPC_H

#include <stdint.h>

// 网络设备 IPC 端口名称
#define NETDEV_PORT_NAME "netdev.ne2000"

// 网络设备操作类型
#define NETDEV_OP_SEND 1    // 发送数据包
#define NETDEV_OP_RECV 2    // 接收数据包
#define NETDEV_OP_GET_MAC 3 // 获取 MAC 地址
#define NETDEV_OP_SET_MAC 4 // 设置 MAC 地址

// 网络设备状态
#define NETDEV_STATUS_OK 0      // 成功
#define NETDEV_STATUS_ERROR 1   // 错误
#define NETDEV_STATUS_TIMEOUT 2 // 超时
#define NETDEV_STATUS_INVALID 3 // 无效操作

// 最大数据包大小
#define NETDEV_MAX_PACKET_SIZE 1518

// 网络设备请求
typedef struct
{
    uint32_t request_id;  // 请求 ID
    uint32_t operation;   // 操作类型
    uint32_t length;      // 数据长度
    uint32_t buffer_addr; // 数据缓冲区地址
    uint8_t mac_addr[6];  // MAC 地址（用于 SET_MAC）
    uint8_t reserved[2];  // 对齐
} netdev_request_t;

// 网络设备响应
typedef struct
{
    uint32_t request_id;        // 对应的请求 ID
    uint32_t status;            // 状态
    uint32_t bytes_transferred; // 传输的字节数
    uint8_t mac_addr[6];        // MAC 地址（用于 GET_MAC）
    uint8_t reserved[2];        // 对齐
} netdev_response_t;

#endif // NETDEV_IPC_H
