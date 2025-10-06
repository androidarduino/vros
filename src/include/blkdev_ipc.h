#ifndef BLKDEV_IPC_H
#define BLKDEV_IPC_H

#include <stdint.h>

// 块设备 IPC 协议定义
// 用于内核与用户空间块设备驱动通信

// 块设备请求类型
#define BLKDEV_OP_READ 1
#define BLKDEV_OP_WRITE 2
#define BLKDEV_OP_FLUSH 3
#define BLKDEV_OP_IDENTIFY 4

// 块设备响应状态
#define BLKDEV_STATUS_OK 0
#define BLKDEV_STATUS_ERROR 1
#define BLKDEV_STATUS_INVALID 2

// 块设备请求消息（内核 -> 驱动）
typedef struct
{
    uint32_t request_id;  // 请求 ID，用于匹配响应
    uint32_t operation;   // 操作类型 (BLKDEV_OP_*)
    uint32_t drive;       // 驱动器号 (0=主盘, 1=从盘)
    uint32_t lba;         // 逻辑块地址
    uint32_t count;       // 扇区数量
    uint32_t buffer_addr; // 数据缓冲区地址（用户空间地址）
} blkdev_request_t;

// 块设备响应消息（驱动 -> 内核）
typedef struct
{
    uint32_t request_id;        // 对应的请求 ID
    uint32_t status;            // 状态码 (BLKDEV_STATUS_*)
    uint32_t bytes_transferred; // 传输的字节数
} blkdev_response_t;

// IPC 端口名称
#define BLKDEV_PORT_NAME "blkdev.ata"

#endif // BLKDEV_IPC_H
