#ifndef RAMFS_H
#define RAMFS_H

#include "vfs.h"

// RAM filesystem node
struct ramfs_node
{
    char *data;        // File data
    uint32_t capacity; // Allocated capacity
    uint32_t size;     // Actual data size
};

// Initialize ramfs
void ramfs_init(void);

// Mount ramfs as root
int ramfs_mount_root(void);

// Create a file in ramfs
struct inode *ramfs_create_file(const char *path, const char *initial_content);

#endif // RAMFS_H
