#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// ATA I/O ports (Primary bus)
#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CONTROL 0x3F6
#define ATA_PRIMARY_IRQ 14

// ATA I/O ports (Secondary bus)
#define ATA_SECONDARY_IO 0x170
#define ATA_SECONDARY_CONTROL 0x376
#define ATA_SECONDARY_IRQ 15

// ATA registers
#define ATA_REG_DATA 0      // Read/Write data
#define ATA_REG_ERROR 1     // Read error info
#define ATA_REG_FEATURES 1  // Write features
#define ATA_REG_SECCOUNT0 2 // Sector count
#define ATA_REG_LBA0 3      // LBA low
#define ATA_REG_LBA1 4      // LBA mid
#define ATA_REG_LBA2 5      // LBA high
#define ATA_REG_HDDEVSEL 6  // Drive select
#define ATA_REG_COMMAND 7   // Write command
#define ATA_REG_STATUS 7    // Read status

// ATA status bits
#define ATA_SR_BSY 0x80  // Busy
#define ATA_SR_DRDY 0x40 // Drive ready
#define ATA_SR_DF 0x20   // Drive write fault
#define ATA_SR_DSC 0x10  // Drive seek complete
#define ATA_SR_DRQ 0x08  // Data request ready
#define ATA_SR_CORR 0x04 // Corrected data
#define ATA_SR_IDX 0x02  // Index
#define ATA_SR_ERR 0x01  // Error

// ATA commands
#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_READ_PIO_EXT 0x24
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_WRITE_PIO_EXT 0x34
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_IDENTIFY 0xEC

// ATA device type
#define ATA_MASTER 0
#define ATA_SLAVE 1

// Sector size
#define ATA_SECTOR_SIZE 512

// ATA device structure
struct ata_device
{
    uint16_t io_base;      // I/O base port
    uint16_t control_base; // Control base port
    uint8_t slave;         // 0 = master, 1 = slave
    uint8_t exists;        // Device exists
    char model[41];        // Device model string
    uint32_t size;         // Size in sectors
};

// Function declarations
void ata_init(void);
int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t sectors, void *buffer);
int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t sectors, const void *buffer);
struct ata_device *ata_get_device(uint8_t drive);

#endif // ATA_H
