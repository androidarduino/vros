# VROS 驱动配置系统

## 概述

VROS 使用一个简单的驱动配置表来管理用户空间驱动的启动。所有配置都在一个文件中：`src/kernel/driver_config.c`

## 如何使用

### 1. 启用/禁用现有驱动

编辑 `src/kernel/driver_config.c`，找到驱动表：

```c
struct driver_config driver_table[] = {
    {
        .name = "ata_driver",
        .entry_point = ata_driver_main,
        .enabled = 1,  // 改为 0 可以禁用
        .description = "ATA/IDE disk driver (user-space)"
    },
    {
        .name = "ne2000_driver",
        .entry_point = ne2000_driver_main,
        .enabled = 1,  // 改为 0 可以禁用
        .description = "NE2000 network driver (user-space)"
    },
};
```

**修改 `enabled` 字段：**
- `enabled = 1` - 驱动会在系统启动时自动启动
- `enabled = 0` - 驱动不会自动启动

### 2. 添加新驱动

#### 步骤 1：创建驱动代码

创建新驱动文件（例如 `src/lib/usb_driver.c`）：

```c
void usb_driver_main(void)
{
    // 驱动代码...
    while (1) {
        // 消息循环
    }
}
```

#### 步骤 2：在配置表中添加

编辑 `src/kernel/driver_config.c`：

1. **添加函数声明：**
```c
extern void ata_driver_main(void);
extern void ne2000_driver_main(void);
extern void usb_driver_main(void);  // 新增这行
```

2. **在驱动表中添加条目：**
```c
struct driver_config driver_table[] = {
    // ... 现有驱动 ...
    {
        .name = "usb_driver",
        .entry_point = usb_driver_main,
        .enabled = 1,  // 设为 1 启用
        .description = "USB host controller driver"
    },
};
```

#### 步骤 3：更新 Makefile

在 `Makefile` 中添加新驱动的源文件：

```makefile
SRCS_C := ... \
          $(LIB_DIR)/ata_driver.c \
          $(LIB_DIR)/ne2000_driver.c \
          $(LIB_DIR)/usb_driver.c    # 新增这行
```

#### 步骤 4：编译运行

```bash
make clean
make run
```

## 配置示例

### 示例 1：只启用 ATA 驱动（禁用网络）

```c
struct driver_config driver_table[] = {
    {
        .name = "ata_driver",
        .entry_point = ata_driver_main,
        .enabled = 1,  // 启用
        .description = "ATA/IDE disk driver (user-space)"
    },
    {
        .name = "ne2000_driver",
        .entry_point = ne2000_driver_main,
        .enabled = 0,  // 禁用
        .description = "NE2000 network driver (user-space)"
    },
};
```

### 示例 2：禁用所有驱动

```c
struct driver_config driver_table[] = {
    {
        .name = "ata_driver",
        .entry_point = ata_driver_main,
        .enabled = 0,  // 禁用
        .description = "ATA/IDE disk driver (user-space)"
    },
    {
        .name = "ne2000_driver",
        .entry_point = ne2000_driver_main,
        .enabled = 0,  // 禁用
        .description = "NE2000 network driver (user-space)"
    },
};
```

然后可以在 Shell 中手动启动：
```bash
> atadrv    # 手动启动 ATA 驱动
> netdrv    # 手动启动 NE2000 驱动
```

## 优点

1. **集中配置** - 所有驱动配置在一个文件中
2. **易于修改** - 只需改变 `enabled` 字段
3. **描述信息** - 每个驱动都有描述，便于理解
4. **易于扩展** - 添加新驱动只需在表中加一行
5. **编译时配置** - 不需要运行时解析配置文件

## 未来扩展

可能的改进方向：

1. **运行时配置文件** - 从 `/etc/drivers.conf` 读取配置
2. **驱动优先级** - 控制启动顺序
3. **依赖关系** - 驱动之间的依赖管理
4. **热插拔** - 运行时加载/卸载驱动
5. **Shell 命令** - `drv list`, `drv start`, `drv stop` 等

## 与硬编码方案的对比

### 旧方案（硬编码）：
```c
// kernel.c 中
task_create("ata_driver", ata_driver_main);
task_create("ne2000_driver", ne2000_driver_main);
// 添加新驱动需要修改 kernel.c
```

**缺点：**
- 修改内核主文件
- 难以管理
- 不易扩展

### 新方案（配置表）：
```c
// driver_config.c 中
struct driver_config driver_table[] = {
    {.name = "ata_driver", .entry_point = ata_driver_main, .enabled = 1},
    {.name = "ne2000_driver", .entry_point = ne2000_driver_main, .enabled = 1},
    // 添加新驱动只需加一行
};
```

**优点：**
- 独立配置文件
- 易于管理
- 易于扩展
- 可以快速启用/禁用

## 总结

这个配置系统提供了一个简单但强大的方式来管理用户空间驱动。通过修改一个配置文件，就可以轻松控制系统启动时加载哪些驱动。

