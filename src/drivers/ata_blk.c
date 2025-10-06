#include "ata.h"
#include "blkdev.h"
#include "kmalloc.h"

// ATA block device private data
struct ata_blk_private
{
    uint8_t drive;
};

// Create wrapper functions for each drive
static struct
{
    int (*read)(uint32_t, void *);
    int (*write)(uint32_t, const void *);
} ata_wrappers[4];

// Drive 0 wrappers
static int ata0_read(uint32_t block, void *buffer)
{
    return ata_read_sectors(0, block, 1, buffer);
}
static int ata0_write(uint32_t block, const void *buffer)
{
    return ata_write_sectors(0, block, 1, buffer);
}

// Drive 1 wrappers
static int ata1_read(uint32_t block, void *buffer)
{
    return ata_read_sectors(1, block, 1, buffer);
}
static int ata1_write(uint32_t block, const void *buffer)
{
    return ata_write_sectors(1, block, 1, buffer);
}

// Drive 2 wrappers
static int ata2_read(uint32_t block, void *buffer)
{
    return ata_read_sectors(2, block, 1, buffer);
}
static int ata2_write(uint32_t block, const void *buffer)
{
    return ata_write_sectors(2, block, 1, buffer);
}

// Drive 3 wrappers
static int ata3_read(uint32_t block, void *buffer)
{
    return ata_read_sectors(3, block, 1, buffer);
}
static int ata3_write(uint32_t block, const void *buffer)
{
    return ata_write_sectors(3, block, 1, buffer);
}

// Register ATA devices as block devices
void ata_register_block_devices(void)
{
    extern struct ata_device *ata_get_device(uint8_t drive);
    extern int blkdev_register(const char *, uint32_t,
                               int (*)(uint32_t, void *),
                               int (*)(uint32_t, const void *),
                               void *);
    extern void print_string(const char *str, int row);

    // Setup function pointers
    ata_wrappers[0].read = ata0_read;
    ata_wrappers[0].write = ata0_write;
    ata_wrappers[1].read = ata1_read;
    ata_wrappers[1].write = ata1_write;
    ata_wrappers[2].read = ata2_read;
    ata_wrappers[2].write = ata2_write;
    ata_wrappers[3].read = ata3_read;
    ata_wrappers[3].write = ata3_write;

    int registered = 0;
    for (int i = 0; i < 4; i++)
    {
        struct ata_device *dev = ata_get_device(i);
        if (dev && dev->exists)
        {
            char name[4];
            name[0] = 'h';
            name[1] = 'd';
            name[2] = 'a' + i;
            name[3] = '\0';

            blkdev_register(name, dev->size, ata_wrappers[i].read, ata_wrappers[i].write, 0);
            registered++;
        }
    }

    // Debug output
    if (registered == 0)
    {
        print_string("Warning: No ATA devices found!", 28);
    }
}
