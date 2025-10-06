#include "ata.h"
#include "port_io.h"
#include <stdint.h>

// ATA devices (4 possible: primary master/slave, secondary master/slave)
static struct ata_device ata_devices[4];

// Helper: Read ATA register
static uint8_t ata_read_reg(struct ata_device *dev, uint8_t reg)
{
    return inb(dev->io_base + reg);
}

// Helper: Write ATA register
static void ata_write_reg(struct ata_device *dev, uint8_t reg, uint8_t data)
{
    outb(dev->io_base + reg, data);
}

// Helper: Wait for BSY to clear
static int ata_wait_bsy(struct ata_device *dev)
{
    uint8_t status;
    int timeout = 100000;

    while (timeout-- > 0)
    {
        status = ata_read_reg(dev, ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY))
            return 0;
    }

    return -1; // Timeout
}

// Helper: Wait for DRQ to set
static int ata_wait_drq(struct ata_device *dev)
{
    uint8_t status;
    int timeout = 100000;

    while (timeout-- > 0)
    {
        status = ata_read_reg(dev, ATA_REG_STATUS);
        if (status & ATA_SR_DRQ)
            return 0;
    }

    return -1; // Timeout
}

// Helper: Select drive
static void ata_select_drive(struct ata_device *dev)
{
    ata_write_reg(dev, ATA_REG_HDDEVSEL, 0xA0 | (dev->slave << 4));

    // 400ns delay
    for (int i = 0; i < 4; i++)
        ata_read_reg(dev, ATA_REG_STATUS);
}

// Detect ATA device
static int ata_detect(struct ata_device *dev)
{
    ata_select_drive(dev);
    ata_wait_bsy(dev);

    // Send IDENTIFY command
    ata_write_reg(dev, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    // Check if device exists
    uint8_t status = ata_read_reg(dev, ATA_REG_STATUS);
    if (status == 0)
        return 0; // No device

    // Wait for BSY to clear
    if (ata_wait_bsy(dev) < 0)
        return 0;

    // Wait for DRQ
    if (ata_wait_drq(dev) < 0)
        return 0;

    // Read identification data
    uint16_t identify[256];
    for (int i = 0; i < 256; i++)
        identify[i] = inw(dev->io_base + ATA_REG_DATA);

    // Extract model string (words 27-46)
    for (int i = 0; i < 40; i += 2)
    {
        dev->model[i] = identify[27 + i / 2] >> 8;
        dev->model[i + 1] = identify[27 + i / 2] & 0xFF;
    }
    dev->model[40] = '\0';

    // Extract size (words 60-61 for 28-bit LBA)
    dev->size = (identify[61] << 16) | identify[60];

    dev->exists = 1;
    return 1;
}

// Initialize ATA subsystem
void ata_init(void)
{
    // Initialize device structures
    for (int i = 0; i < 4; i++)
    {
        ata_devices[i].exists = 0;
        ata_devices[i].size = 0;
    }

    // Primary bus
    ata_devices[0].io_base = ATA_PRIMARY_IO;
    ata_devices[0].control_base = ATA_PRIMARY_CONTROL;
    ata_devices[0].slave = ATA_MASTER;

    ata_devices[1].io_base = ATA_PRIMARY_IO;
    ata_devices[1].control_base = ATA_PRIMARY_CONTROL;
    ata_devices[1].slave = ATA_SLAVE;

    // Secondary bus
    ata_devices[2].io_base = ATA_SECONDARY_IO;
    ata_devices[2].control_base = ATA_SECONDARY_CONTROL;
    ata_devices[2].slave = ATA_MASTER;

    ata_devices[3].io_base = ATA_SECONDARY_IO;
    ata_devices[3].control_base = ATA_SECONDARY_CONTROL;
    ata_devices[3].slave = ATA_SLAVE;

    // Detect devices
    for (int i = 0; i < 4; i++)
    {
        if (ata_detect(&ata_devices[i]))
        {
            // Device found (we'll log this later)
        }
    }
}

// Get ATA device by drive number
struct ata_device *ata_get_device(uint8_t drive)
{
    if (drive >= 4)
        return 0;

    if (!ata_devices[drive].exists)
        return 0;

    return &ata_devices[drive];
}

// Read sectors from ATA device
int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t sectors, void *buffer)
{
    if (drive >= 4 || !ata_devices[drive].exists)
        return -1;

    struct ata_device *dev = &ata_devices[drive];
    uint16_t *buf = (uint16_t *)buffer;

    // Select drive
    ata_select_drive(dev);

    if (ata_wait_bsy(dev) < 0)
        return -1;

    // Set up LBA and sector count
    ata_write_reg(dev, ATA_REG_SECCOUNT0, sectors);
    ata_write_reg(dev, ATA_REG_LBA0, (uint8_t)(lba));
    ata_write_reg(dev, ATA_REG_LBA1, (uint8_t)(lba >> 8));
    ata_write_reg(dev, ATA_REG_LBA2, (uint8_t)(lba >> 16));
    ata_write_reg(dev, ATA_REG_HDDEVSEL, 0xE0 | (dev->slave << 4) | ((lba >> 24) & 0x0F));

    // Send READ command
    ata_write_reg(dev, ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    // Read each sector
    for (int sector = 0; sector < sectors; sector++)
    {
        if (ata_wait_drq(dev) < 0)
            return -1;

        // Read 256 words (512 bytes)
        for (int i = 0; i < 256; i++)
            buf[sector * 256 + i] = inw(dev->io_base + ATA_REG_DATA);
    }

    return 0;
}

// Write sectors to ATA device
int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t sectors, const void *buffer)
{
    if (drive >= 4 || !ata_devices[drive].exists)
        return -1;

    struct ata_device *dev = &ata_devices[drive];
    const uint16_t *buf = (const uint16_t *)buffer;

    // Debug: print first 4 bytes being written if LBA is 0
    if (lba == 0)
    {
        extern void print_string(const char *str, int row);
        const uint32_t *data = (const uint32_t *)buffer;
        char msg[32];
        const char *hex = "0123456789ABCDEF";
        msg[0] = 'W';
        msg[1] = 'r';
        msg[2] = 'i';
        msg[3] = 't';
        msg[4] = 'e';
        msg[5] = ':';
        msg[6] = ' ';
        msg[7] = '0';
        msg[8] = 'x';
        for (int i = 7; i >= 0; i--)
            msg[9 + (7 - i)] = hex[(data[0] >> (i * 4)) & 0xF];
        msg[17] = '\0';
        print_string(msg, 38);
    }

    // Select drive
    ata_select_drive(dev);

    if (ata_wait_bsy(dev) < 0)
        return -1;

    // Set up LBA and sector count
    ata_write_reg(dev, ATA_REG_SECCOUNT0, sectors);
    ata_write_reg(dev, ATA_REG_LBA0, (uint8_t)(lba));
    ata_write_reg(dev, ATA_REG_LBA1, (uint8_t)(lba >> 8));
    ata_write_reg(dev, ATA_REG_LBA2, (uint8_t)(lba >> 16));
    ata_write_reg(dev, ATA_REG_HDDEVSEL, 0xE0 | (dev->slave << 4) | ((lba >> 24) & 0x0F));

    // Send WRITE command
    ata_write_reg(dev, ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    // Write each sector
    for (int sector = 0; sector < sectors; sector++)
    {
        if (ata_wait_drq(dev) < 0)
            return -1;

        // Write 256 words (512 bytes)
        for (int i = 0; i < 256; i++)
            outw(dev->io_base + ATA_REG_DATA, buf[sector * 256 + i]);

        // Wait for the write to complete
        if (ata_wait_bsy(dev) < 0)
            return -1;
    }

    // Flush cache after all sectors are written
    ata_write_reg(dev, ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    if (ata_wait_bsy(dev) < 0)
        return -1;

    return 0;
}
