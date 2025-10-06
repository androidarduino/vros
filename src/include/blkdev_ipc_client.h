#ifndef BLKDEV_IPC_CLIENT_H
#define BLKDEV_IPC_CLIENT_H

#include <stdint.h>

// 块设备 IPC 客户端接口
// 内核使用此接口与用户空间块设备驱动通信

// 初始化块设备 IPC 客户端
int blkdev_ipc_client_init(void);

// 读取扇区
// 返回读取的字节数，失败返回 -1
int blkdev_ipc_read(uint8_t drive, uint32_t lba, uint32_t count, void *buffer);

// 写入扇区
// 返回写入的字节数，失败返回 -1
int blkdev_ipc_write(uint8_t drive, uint32_t lba, uint32_t count, const void *buffer);

// 刷新缓存
// 成功返回 0，失败返回 -1
int blkdev_ipc_flush(uint8_t drive);

// 检查驱动是否可用
// 可用返回 1，不可用返回 0
int blkdev_ipc_driver_available(void);

#endif // BLKDEV_IPC_CLIENT_H
