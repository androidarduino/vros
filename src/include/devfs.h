#ifndef DEVFS_H
#define DEVFS_H

#include "vfs.h"

// Device types
typedef enum
{
    DEV_NULL,
    DEV_ZERO,
    DEV_RANDOM,
} devfs_device_type_t;

// Device node
struct devfs_node
{
    devfs_device_type_t type;
    void *private_data;
};

// Device operations (similar to file_operations but device-specific)
struct device_operations
{
    int (*read)(void *private_data, char *buffer, uint32_t size, uint32_t offset);
    int (*write)(void *private_data, const char *buffer, uint32_t size, uint32_t offset);
};

// Initialize devfs
void devfs_init(void);

// Mount devfs at /dev
int devfs_mount(void);

// Register a device
int devfs_register_device(const char *name, devfs_device_type_t type, struct device_operations *ops);

#endif // DEVFS_H
