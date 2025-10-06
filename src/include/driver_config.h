#ifndef DRIVER_CONFIG_H
#define DRIVER_CONFIG_H

#include <stdint.h>

// 驱动配置项
struct driver_config
{
    const char *name;          // 驱动名称
    void (*entry_point)(void); // 驱动入口函数
    int enabled;               // 是否启用 (1=启用, 0=禁用)
    const char *description;   // 描述信息
};

// 驱动配置表（在 driver_config.c 中定义）
extern struct driver_config driver_table[];
extern int driver_table_size;

// 启动所有已启用的驱动
void driver_config_init(void);

#endif
