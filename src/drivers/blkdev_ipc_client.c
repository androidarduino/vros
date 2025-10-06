/*
 * 块设备 IPC 客户端
 *
 * 内核侧的块设备 IPC 客户端，用于与用户空间驱动通信
 */

#include <stdint.h>
#include "blkdev_ipc.h"
#include "ipc.h"
#include "kmalloc.h"

// 全局请求 ID 计数器
static uint32_t next_request_id = 1;

// IPC 客户端端口
int client_port = -1; // 移除 static 用于调试

// 初始化块设备 IPC 客户端
int blkdev_ipc_client_init(void)
{
    extern void print_string(const char *str, int row);

    print_string("Creating IPC client port...", 45);

    // 创建客户端 IPC 端口用于接收响应
    client_port = ipc_create_port();
    if (client_port < 0)
    {
        print_string(" FAILED!", 45);
        return -1;
    }

    print_string(" OK, port=", 45);
    if (client_port == 0)
        print_string("0", 45);
    else if (client_port == 1)
        print_string("1", 45);
    else if (client_port == 2)
        print_string("2", 45);
    else
        print_string("?", 45);

    return 0;
}

// 通过 IPC 读取扇区
int blkdev_ipc_read(uint8_t drive, uint32_t lba, uint32_t count, void *buffer)
{
    if (client_port < 0)
    {
        return -1; // 未初始化
    }

    // 查找驱动端口
    int driver_port = ipc_find_port(BLKDEV_PORT_NAME);
    if (driver_port < 0)
    {
        return -1; // 驱动未运行
    }

    // 构造请求
    blkdev_request_t req;
    req.request_id = next_request_id++;
    req.operation = BLKDEV_OP_READ;
    req.drive = drive;
    req.lba = lba;
    req.count = count;
    req.buffer_addr = (uint32_t)buffer;

    // 发送请求 - 使用 client_port 作为源端口
    if (ipc_send_from_port(client_port, driver_port, 0, &req, sizeof(req)) != 0)
    {
        return -1;
    }

    // 等待响应
    struct ipc_message msg;
    if (ipc_recv(client_port, &msg) != 0)
    {
        return -1;
    }

    // 检查响应
    if (msg.size < sizeof(blkdev_response_t))
    {
        return -1;
    }

    blkdev_response_t *resp = (blkdev_response_t *)msg.data;

    if (resp->request_id != req.request_id)
    {
        return -1; // 响应不匹配
    }

    if (resp->status != BLKDEV_STATUS_OK)
    {
        return -1; // 操作失败
    }

    return resp->bytes_transferred;
}

// 通过 IPC 写入扇区
int blkdev_ipc_write(uint8_t drive, uint32_t lba, uint32_t count, const void *buffer)
{
    if (client_port < 0)
    {
        return -1; // 未初始化
    }

    // 查找驱动端口
    int driver_port = ipc_find_port(BLKDEV_PORT_NAME);
    if (driver_port < 0)
    {
        return -1; // 驱动未运行
    }

    // 构造请求
    blkdev_request_t req;
    req.request_id = next_request_id++;
    req.operation = BLKDEV_OP_WRITE;
    req.drive = drive;
    req.lba = lba;
    req.count = count;
    req.buffer_addr = (uint32_t)buffer;

    // 发送请求
    if (ipc_send_from_port(client_port, driver_port, 0, &req, sizeof(req)) != 0)
    {
        return -1;
    }

    // 等待响应
    struct ipc_message msg;
    if (ipc_recv(client_port, &msg) != 0)
    {
        return -1;
    }

    // 检查响应
    if (msg.size < sizeof(blkdev_response_t))
    {
        return -1;
    }

    blkdev_response_t *resp = (blkdev_response_t *)msg.data;

    if (resp->request_id != req.request_id)
    {
        return -1; // 响应不匹配
    }

    if (resp->status != BLKDEV_STATUS_OK)
    {
        return -1; // 操作失败
    }

    return resp->bytes_transferred;
}

// 刷新缓存
int blkdev_ipc_flush(uint8_t drive)
{
    if (client_port < 0)
    {
        return -1; // 未初始化
    }

    // 查找驱动端口
    int driver_port = ipc_find_port(BLKDEV_PORT_NAME);
    if (driver_port < 0)
    {
        return -1; // 驱动未运行
    }

    // 构造请求
    blkdev_request_t req;
    req.request_id = next_request_id++;
    req.operation = BLKDEV_OP_FLUSH;
    req.drive = drive;
    req.lba = 0;
    req.count = 0;
    req.buffer_addr = 0;

    // 发送请求
    if (ipc_send_from_port(client_port, driver_port, 0, &req, sizeof(req)) != 0)
    {
        return -1;
    }

    // 等待响应
    struct ipc_message msg;
    if (ipc_recv(client_port, &msg) != 0)
    {
        return -1;
    }

    // 检查响应
    if (msg.size < sizeof(blkdev_response_t))
    {
        return -1;
    }

    blkdev_response_t *resp = (blkdev_response_t *)msg.data;

    if (resp->request_id != req.request_id)
    {
        return -1; // 响应不匹配
    }

    if (resp->status != BLKDEV_STATUS_OK)
    {
        return -1; // 操作失败
    }

    return 0;
}

// 检查驱动是否可用
int blkdev_ipc_driver_available(void)
{
    return ipc_find_port(BLKDEV_PORT_NAME) >= 0;
}
