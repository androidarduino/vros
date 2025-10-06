#include "mount.h"
#include "blkdev.h"
#include "simplefs.h"
#include "kmalloc.h"
#include <stdint.h>

// Mount table
struct mount_point mount_table[MAX_MOUNT_POINTS];

// String functions
static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static void strncpy_safe(char *dest, const char *src, int n)
{
    int i;
    for (i = 0; i < n - 1 && src[i]; i++)
        dest[i] = src[i];
    dest[i] = '\0';
}

// Initialize mount system
void mount_init(void)
{
    for (int i = 0; i < MAX_MOUNT_POINTS; i++)
    {
        mount_table[i].in_use = 0;
        mount_table[i].sb = 0;
        mount_table[i].bdev = 0;
    }
}

// Mount a filesystem
int mount_fs(const char *device, const char *path, const char *fstype)
{
    // Check if path is already mounted
    for (int i = 0; i < MAX_MOUNT_POINTS; i++)
    {
        if (mount_table[i].in_use && strcmp(mount_table[i].path, path) == 0)
        {
            return -2; // Already mounted
        }
    }

    // Find free mount point
    int slot = -1;
    for (int i = 0; i < MAX_MOUNT_POINTS; i++)
    {
        if (!mount_table[i].in_use)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
        return -1; // No free slots

    // Get block device
    struct block_device *bdev = blkdev_get(device);
    if (!bdev)
        return -1;

    // Mount filesystem based on type
    struct superblock *sb = 0;

    if (strcmp(fstype, "simplefs") == 0)
    {
        sb = simplefs_mount(bdev);
    }
    else
    {
        return -1; // Unknown filesystem
    }

    if (!sb)
        return -1; // Mount failed

    // Register mount point
    strncpy_safe(mount_table[slot].path, path, 256);
    mount_table[slot].sb = sb;
    mount_table[slot].bdev = bdev;
    mount_table[slot].in_use = 1;

    return 0;
}

// Unmount a filesystem
int unmount_fs(const char *path)
{
    // Find mount point
    for (int i = 0; i < MAX_MOUNT_POINTS; i++)
    {
        if (mount_table[i].in_use && strcmp(mount_table[i].path, path) == 0)
        {
            // Unmount
            if (mount_table[i].sb)
            {
                simplefs_unmount(mount_table[i].sb);
            }

            mount_table[i].in_use = 0;
            mount_table[i].sb = 0;
            mount_table[i].bdev = 0;

            return 0;
        }
    }

    return -1; // Not found
}

// Get superblock for a mount point
struct superblock *mount_get_sb(const char *path)
{
    for (int i = 0; i < MAX_MOUNT_POINTS; i++)
    {
        if (mount_table[i].in_use && strcmp(mount_table[i].path, path) == 0)
        {
            return mount_table[i].sb;
        }
    }

    return 0;
}
