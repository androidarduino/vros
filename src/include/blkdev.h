#ifndef BLKDEV_H
#define BLKDEV_H

#include <stdint.h>

#define BLOCK_SIZE 512
#define MAX_BLOCK_DEVICES 8

// Block device structure
struct block_device
{
    char name[16];  // Device name (e.g., "hda")
    uint8_t in_use; // Device is registered
    uint32_t size;  // Size in blocks

    // Operations
    int (*read)(uint32_t block, void *buffer);
    int (*write)(uint32_t block, const void *buffer);

    void *private_data; // Driver-specific data
};

// Block device functions
void blkdev_init(void);
int blkdev_register(const char *name, uint32_t size,
                    int (*read)(uint32_t, void *),
                    int (*write)(uint32_t, const void *),
                    void *private_data);
struct block_device *blkdev_get(const char *name);
int blkdev_read(struct block_device *dev, uint32_t block, void *buffer);
int blkdev_write(struct block_device *dev, uint32_t block, const void *buffer);

#endif // BLKDEV_H
