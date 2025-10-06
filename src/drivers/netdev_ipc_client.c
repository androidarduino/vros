/*
 * 网络设备 IPC 客户端
 *
 * 内核侧的网络设备 IPC 客户端，用于与用户空间驱动通信
 */

#include <stdint.h>
#include "netdev_ipc.h"
#include "ipc.h"
#include "kmalloc.h"

// 全局请求 ID 计数器
static uint32_t next_request_id = 1;

// IPC 客户端端口
int netdev_client_port = -1;

// 初始化网络设备 IPC 客户端
int netdev_ipc_client_init(void)
{
    extern void print_string(const char *str, int row);

    print_string("Creating network IPC client port...", 46);

    // 创建客户端 IPC 端口用于接收响应
    netdev_client_port = ipc_create_port();
    if (netdev_client_port < 0)
    {
        print_string(" FAILED!", 46);
        return -1;
    }

    print_string(" OK", 46);

    return 0;
}

// 通过 IPC 发送数据包
int netdev_ipc_send(const uint8_t *data, uint32_t length)
{
    if (netdev_client_port < 0)
    {
        return -1; // 未初始化
    }

    // 查找驱动端口
    int driver_port = ipc_find_port(NETDEV_PORT_NAME);
    if (driver_port < 0)
    {
        return -1; // 驱动未运行
    }

    // 构造请求
    netdev_request_t req;
    req.request_id = next_request_id++;
    req.operation = NETDEV_OP_SEND;
    req.length = length;
    req.buffer_addr = (uint32_t)data;

    // 发送请求
    if (ipc_send_from_port(netdev_client_port, driver_port, 0, &req, sizeof(req)) != 0)
    {
        return -1;
    }

    // 等待响应
    struct ipc_message msg;
    if (ipc_recv(netdev_client_port, &msg) != 0)
    {
        return -1;
    }

    // 检查响应
    if (msg.size < sizeof(netdev_response_t))
    {
        return -1;
    }

    netdev_response_t *resp = (netdev_response_t *)msg.data;

    if (resp->request_id != req.request_id)
    {
        return -1; // 响应不匹配
    }

    if (resp->status != NETDEV_STATUS_OK)
    {
        return -1; // 操作失败
    }

    return resp->bytes_transferred;
}

// 通过 IPC 接收数据包
int netdev_ipc_recv(uint8_t *buffer, uint32_t max_length)
{
    if (netdev_client_port < 0)
    {
        return -1; // 未初始化
    }

    // 查找驱动端口
    int driver_port = ipc_find_port(NETDEV_PORT_NAME);
    if (driver_port < 0)
    {
        return -1; // 驱动未运行
    }

    // 构造请求
    netdev_request_t req;
    req.request_id = next_request_id++;
    req.operation = NETDEV_OP_RECV;
    req.length = max_length;
    req.buffer_addr = (uint32_t)buffer;

    // 发送请求
    if (ipc_send_from_port(netdev_client_port, driver_port, 0, &req, sizeof(req)) != 0)
    {
        return -1;
    }

    // 等待响应
    struct ipc_message msg;
    if (ipc_recv(netdev_client_port, &msg) != 0)
    {
        return -1;
    }

    // 检查响应
    if (msg.size < sizeof(netdev_response_t))
    {
        return -1;
    }

    netdev_response_t *resp = (netdev_response_t *)msg.data;

    if (resp->request_id != req.request_id)
    {
        return -1; // 响应不匹配
    }

    if (resp->status == NETDEV_STATUS_TIMEOUT)
    {
        return 0; // 没有数据包
    }

    if (resp->status != NETDEV_STATUS_OK)
    {
        return -1; // 操作失败
    }

    return resp->bytes_transferred;
}

// 通过 IPC 获取 MAC 地址
int netdev_ipc_get_mac(uint8_t *mac)
{
    if (netdev_client_port < 0)
    {
        return -1; // 未初始化
    }

    // 查找驱动端口
    int driver_port = ipc_find_port(NETDEV_PORT_NAME);
    if (driver_port < 0)
    {
        return -1; // 驱动未运行
    }

    // 构造请求
    netdev_request_t req;
    req.request_id = next_request_id++;
    req.operation = NETDEV_OP_GET_MAC;
    req.length = 0;
    req.buffer_addr = 0;

    // 发送请求
    if (ipc_send_from_port(netdev_client_port, driver_port, 0, &req, sizeof(req)) != 0)
    {
        return -1;
    }

    // 等待响应
    struct ipc_message msg;
    if (ipc_recv(netdev_client_port, &msg) != 0)
    {
        return -1;
    }

    // 检查响应
    if (msg.size < sizeof(netdev_response_t))
    {
        return -1;
    }

    netdev_response_t *resp = (netdev_response_t *)msg.data;

    if (resp->request_id != req.request_id)
    {
        return -1; // 响应不匹配
    }

    if (resp->status != NETDEV_STATUS_OK)
    {
        return -1; // 操作失败
    }

    // 复制 MAC 地址
    for (int i = 0; i < 6; i++)
    {
        mac[i] = resp->mac_addr[i];
    }

    return 0;
}

// 检查驱动是否可用
int netdev_ipc_driver_available(void)
{
    return ipc_find_port(NETDEV_PORT_NAME) >= 0;
}
