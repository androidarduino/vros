#ifndef SIMPLEFS_H
#define SIMPLEFS_H

#include <stdint.h>
#include "vfs.h"
#include "blkdev.h"

// SimpleFS magic number
#define SIMPLEFS_MAGIC 0x53465301 // "SFS\x01"

// Block sizes and counts
#define SIMPLEFS_BLOCK_SIZE 512
#define SIMPLEFS_MAX_INODES 128
#define SIMPLEFS_MAX_BLOCKS 1024
#define SIMPLEFS_MAX_NAME 28
#define SIMPLEFS_DIRECT_BLOCKS 12

// Inode types
#define SIMPLEFS_INODE_FILE 1
#define SIMPLEFS_INODE_DIR 2

// Superblock (block 0)
struct simplefs_superblock
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
struct simplefs_inode
{
    uint16_t mode;                           // File type and permissions
    uint16_t links_count;                    // Hard link count
    uint32_t size;                           // Size in bytes
    uint32_t blocks;                         // Blocks allocated
    uint32_t direct[SIMPLEFS_DIRECT_BLOCKS]; // Direct block pointers
    uint32_t indirect;                       // Indirect block pointer
    char padding[12];                        // Pad to 64 bytes
};

// Directory entry
struct simplefs_dirent
{
    uint32_t inode;               // Inode number
    char name[SIMPLEFS_MAX_NAME]; // File name
};

// In-memory superblock info
struct simplefs_sb_info
{
    struct simplefs_superblock sb;
    struct block_device *bdev;
    uint8_t *inode_bitmap; // Cached inode bitmap
    uint8_t *block_bitmap; // Cached block bitmap
};

// In-memory inode info
struct simplefs_inode_info
{
    struct simplefs_inode disk_inode;
    uint32_t inode_no;
};

// Function declarations
int simplefs_init(void);
int simplefs_mkfs(struct block_device *bdev);
struct superblock *simplefs_mount(struct block_device *bdev);
int simplefs_unmount(struct superblock *sb);

#endif // SIMPLEFS_H
