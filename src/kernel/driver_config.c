#include "driver_config.h"

// 驱动入口函数声明
extern void ata_driver_main(void);
extern void ne2000_driver_main(void);
// 未来可以添加更多驱动...

// 驱动配置表
// 要添加新驱动：只需在这里添加一行，设置 enabled=1
// 要禁用驱动：设置 enabled=0
struct driver_config driver_table[] = {
    {.name = "ata_driver",
     .entry_point = ata_driver_main,
     .enabled = 1, // 修改这里可以启用/禁用驱动
     .description = "ATA/IDE disk driver (user-space)"},
    {.name = "ne2000_driver",
     .entry_point = ne2000_driver_main,
     .enabled = 1, // 修改这里可以启用/禁用驱动
     .description = "NE2000 network driver (user-space)"},
    // 添加新驱动示例（已禁用）：
    // {
    //     .name = "usb_driver",
    //     .entry_point = usb_driver_main,
    //     .enabled = 0,
    //     .description = "USB host controller driver"
    // },
};

// 驱动表大小
int driver_table_size = sizeof(driver_table) / sizeof(driver_table[0]);

// 启动所有已启用的驱动
void driver_config_init(void)
{
    extern void print_string(const char *str, int row);
    extern uint32_t task_create(const char *name, void (*entry_point)(void));

    print_string("Starting user-space drivers...", 38);

    int started = 0;
    for (int i = 0; i < driver_table_size; i++)
    {
        if (driver_table[i].enabled)
        {
            // 创建驱动任务
            uint32_t pid = task_create(driver_table[i].name, driver_table[i].entry_point);
            if (pid > 0)
            {
                started++;
            }
        }
    }

    // 显示启动的驱动数量
    if (started > 0)
    {
        print_string("Drivers started!", 37);
    }
    else
    {
        print_string("No drivers started", 37);
    }
}
