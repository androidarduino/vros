#ifndef MOUNT_H
#define MOUNT_H

#include "vfs.h"
#include "blkdev.h"

// Mount point structure
struct mount_point
{
    char path[256];            // Mount point path
    struct superblock *sb;     // Mounted filesystem superblock
    struct block_device *bdev; // Block device
    int in_use;                // Is this mount point active
};

#define MAX_MOUNT_POINTS 8

// Global mount table
extern struct mount_point mount_table[MAX_MOUNT_POINTS];

// Mount functions
void mount_init(void);
int mount_fs(const char *device, const char *path, const char *fstype);
int unmount_fs(const char *path);
struct superblock *mount_get_sb(const char *path);

#endif // MOUNT_H
