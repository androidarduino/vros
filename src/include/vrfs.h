#ifndef VRFS_H
#define VRFS_H

#include <stdint.h>
#include "vfs.h"
#include "blkdev.h"

// VRFS magic number
#define VRFS_MAGIC 0x56524653 // "VRFS"

// Block sizes and counts
#define VRFS_BLOCK_SIZE 512
#define VRFS_MAX_INODES 128
#define VRFS_MAX_BLOCKS 1024
#define VRFS_MAX_NAME 28
#define VRFS_DIRECT_BLOCKS 12

// Inode types
#define VRFS_INODE_FILE 1
#define VRFS_INODE_DIR 2

// Superblock (block 0)
struct vrfs_superblock
{
    uint32_t magic;              // Magic number
    uint32_t block_count;        // Total blocks
    uint32_t inode_count;        // Total inodes
    uint32_t free_blocks;        // Free blocks
    uint32_t free_inodes;        // Free inodes
    uint32_t inode_bitmap_block; // Inode bitmap block
    uint32_t block_bitmap_block; // Block bitmap block
    uint32_t inode_table_block;  // Inode table start block
    uint32_t data_block_start;   // Data blocks start
    char padding[476];           // Pad to 512 bytes
};

// On-disk inode structure
struct vrfs_inode
{
    uint16_t mode;                       // File type and permissions
    uint16_t links_count;                // Hard link count
    uint32_t size;                       // Size in bytes
    uint32_t blocks;                     // Blocks allocated
    uint32_t direct[VRFS_DIRECT_BLOCKS]; // Direct block pointers
    uint32_t indirect;                   // Indirect block pointer
    char padding[12];                    // Pad to 64 bytes
};

// Directory entry
struct vrfs_dirent
{
    uint32_t inode;           // Inode number
    char name[VRFS_MAX_NAME]; // File name
};

// In-memory superblock info
struct vrfs_sb_info
{
    struct vrfs_superblock sb;
    struct block_device *bdev;
    uint8_t *inode_bitmap; // Cached inode bitmap
    uint8_t *block_bitmap; // Cached block bitmap
};

// In-memory inode info
struct vrfs_inode_info
{
    struct vrfs_inode disk_inode;
    uint32_t inode_no;
};

// Function declarations
int vrfs_init(void);
int vrfs_mkfs(struct block_device *bdev);
struct superblock *vrfs_mount(struct block_device *bdev);
int vrfs_unmount(struct superblock *sb);

#endif // VRFS_H
