#ifndef PROCFS_H
#define PROCFS_H

#include "vfs.h"

// procfs file types
typedef enum
{
    PROCFS_UPTIME,
    PROCFS_MEMINFO,
    PROCFS_TASKS,
} procfs_file_type_t;

// procfs node
struct procfs_node
{
    procfs_file_type_t type;
    char *cached_data;
    uint32_t cached_size;
};

// Initialize procfs
void procfs_init(void);

// Mount procfs at /proc
int procfs_mount(void);

#endif // PROCFS_H
