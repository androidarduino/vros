#include "blkdev.h"
#include <stdint.h>

// Block device table
static struct block_device block_devices[MAX_BLOCK_DEVICES];

// String comparison helper
static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// String copy helper
static void strncpy(char *dest, const char *src, int n)
{
    int i;
    for (i = 0; i < n - 1 && src[i]; i++)
        dest[i] = src[i];
    dest[i] = '\0';
}

// Initialize block device subsystem
void blkdev_init(void)
{
    for (int i = 0; i < MAX_BLOCK_DEVICES; i++)
    {
        block_devices[i].in_use = 0;
        block_devices[i].name[0] = '\0';
    }
}

// Register a block device
int blkdev_register(const char *name, uint32_t size,
                    int (*read)(uint32_t, void *),
                    int (*write)(uint32_t, const void *),
                    void *private_data)
{
    // Find free slot
    for (int i = 0; i < MAX_BLOCK_DEVICES; i++)
    {
        if (!block_devices[i].in_use)
        {
            strncpy(block_devices[i].name, name, 16);
            block_devices[i].size = size;
            block_devices[i].read = read;
            block_devices[i].write = write;
            block_devices[i].private_data = private_data;
            block_devices[i].in_use = 1;
            return i;
        }
    }

    return -1; // No free slots
}

// Get block device by name
struct block_device *blkdev_get(const char *name)
{
    for (int i = 0; i < MAX_BLOCK_DEVICES; i++)
    {
        if (block_devices[i].in_use && strcmp(block_devices[i].name, name) == 0)
            return &block_devices[i];
    }

    return 0; // Not found
}

// Read from block device
int blkdev_read(struct block_device *dev, uint32_t block, void *buffer)
{
    if (!dev || !dev->in_use || !dev->read)
        return -1;

    if (block >= dev->size)
        return -1;

    return dev->read(block, buffer);
}

// Write to block device
int blkdev_write(struct block_device *dev, uint32_t block, const void *buffer)
{
    if (!dev || !dev->in_use || !dev->write)
        return -1;

    if (block >= dev->size)
        return -1;

    return dev->write(block, buffer);
}
