#include "vrfs.h"
#include "vfs.h"
#include "kmalloc.h"
#include <stdint.h>

// Helper: Memory set (avoid conflict with potential system memset)
static void fs_memset(void *ptr, int value, uint32_t num)
{
    uint8_t *p = (uint8_t *)ptr;
    for (uint32_t i = 0; i < num; i++)
        p[i] = (uint8_t)value;
}

// Helper: Memory copy (avoid conflict with potential system memcpy)
static void fs_memcpy(void *dest, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++)
        d[i] = s[i];
}

// Helper: Set bit in bitmap
static void bitmap_set(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] |= (1 << (bit % 8));
}

// Helper: Clear bit in bitmap
static void bitmap_clear(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

// Helper: Test bit in bitmap
static int bitmap_test(uint8_t *bitmap, uint32_t bit)
{
    return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

// Helper: Find first free bit in bitmap
static int bitmap_find_free(uint8_t *bitmap, uint32_t max_bits)
{
    for (uint32_t i = 0; i < max_bits; i++)
    {
        if (!bitmap_test(bitmap, i))
            return i;
    }
    return -1;
}

// Allocate an inode
static int vrfs_alloc_inode(struct vrfs_sb_info *sbi)
{
    if (!sbi || sbi->sb.free_inodes == 0)
        return -1;

    int inode_no = bitmap_find_free(sbi->inode_bitmap, sbi->sb.inode_count);
    if (inode_no < 0)
        return -1;

    bitmap_set(sbi->inode_bitmap, inode_no);
    sbi->sb.free_inodes--;

    // Write inode bitmap back to disk
    blkdev_write(sbi->bdev, sbi->sb.inode_bitmap_block, sbi->inode_bitmap);

    return inode_no;
}

// Allocate a data block
static int vrfs_alloc_block(struct vrfs_sb_info *sbi)
{
    if (!sbi || sbi->sb.free_blocks == 0)
        return -1;

    int block_no = bitmap_find_free(sbi->block_bitmap, sbi->sb.block_count);
    if (block_no < 0)
        return -1;

    bitmap_set(sbi->block_bitmap, block_no);
    sbi->sb.free_blocks--;

    // Write block bitmap back to disk
    blkdev_write(sbi->bdev, sbi->sb.block_bitmap_block, sbi->block_bitmap);

    return block_no;
}

// Write inode to disk
static int vrfs_write_inode(struct vrfs_sb_info *sbi, uint32_t inode_no, struct vrfs_inode *inode_data)
{
    if (!sbi || !inode_data)
        return -1;

    // Calculate which block the inode is in
    uint32_t inodes_per_block = VRFS_BLOCK_SIZE / sizeof(struct vrfs_inode);
    uint32_t block_no = sbi->sb.inode_table_block + (inode_no / inodes_per_block);
    uint32_t offset_in_block = (inode_no % inodes_per_block) * sizeof(struct vrfs_inode);

    // Read the block
    uint8_t *buffer = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!buffer)
        return -1;

    if (blkdev_read(sbi->bdev, block_no, buffer) < 0)
    {
        kfree(buffer);
        return -1;
    }

    // Update inode in buffer
    fs_memcpy(buffer + offset_in_block, inode_data, sizeof(struct vrfs_inode));

    // Write back
    int result = blkdev_write(sbi->bdev, block_no, buffer);
    kfree(buffer);

    return result;
}

// Read inode from disk (exported for shell ls command)
int vrfs_read_inode(struct vrfs_sb_info *sbi, uint32_t inode_no, struct vrfs_inode *inode_data)
{
    if (!sbi || !inode_data)
        return -1;

    // Calculate which block the inode is in
    uint32_t inodes_per_block = VRFS_BLOCK_SIZE / sizeof(struct vrfs_inode);
    uint32_t block_no = sbi->sb.inode_table_block + (inode_no / inodes_per_block);
    uint32_t offset_in_block = (inode_no % inodes_per_block) * sizeof(struct vrfs_inode);

    // Read the block
    uint8_t *buffer = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!buffer)
        return -1;

    if (blkdev_read(sbi->bdev, block_no, buffer) < 0)
    {
        kfree(buffer);
        return -1;
    }

    // Copy inode from buffer
    fs_memcpy(inode_data, buffer + offset_in_block, sizeof(struct vrfs_inode));

    kfree(buffer);

    return 0;
}

// Create a VRFS filesystem on a block device
int vrfs_mkfs(struct block_device *bdev)
{
    if (!bdev)
        return -1;

    // Allocate temporary buffer
    uint8_t *buffer = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!buffer)
        return -1;

    // Create superblock
    struct vrfs_superblock *sb = (struct vrfs_superblock *)buffer;
    fs_memset(sb, 0, sizeof(struct vrfs_superblock));

    sb->magic = VRFS_MAGIC;
    sb->block_count = (bdev->size < VRFS_MAX_BLOCKS) ? bdev->size : VRFS_MAX_BLOCKS;
    sb->inode_count = VRFS_MAX_INODES;
    sb->inode_bitmap_block = 1;
    sb->block_bitmap_block = 2;
    sb->inode_table_block = 3;

    // Inode table: 128 inodes * 64 bytes = 8192 bytes = 16 blocks
    sb->data_block_start = sb->inode_table_block + (VRFS_MAX_INODES * 64 + 511) / 512;
    sb->free_blocks = sb->block_count - sb->data_block_start;
    sb->free_inodes = VRFS_MAX_INODES - 1; // Reserve inode 0 for root

    // Save important values before we reuse buffer
    uint32_t inode_bitmap_block = sb->inode_bitmap_block;
    uint32_t block_bitmap_block = sb->block_bitmap_block;
    uint32_t inode_table_block = sb->inode_table_block;
    uint32_t data_block_start = sb->data_block_start;

    // Write superblock
    if (blkdev_write(bdev, 0, buffer) < 0)
    {
        kfree(buffer);
        return -1;
    }

    // Create inode bitmap (clear all)
    fs_memset(buffer, 0, VRFS_BLOCK_SIZE);
    bitmap_set(buffer, 0); // Reserve inode 0 for root
    if (blkdev_write(bdev, inode_bitmap_block, buffer) < 0)
    {
        kfree(buffer);
        return -1;
    }

    // Create block bitmap (mark metadata blocks as used)
    fs_memset(buffer, 0, VRFS_BLOCK_SIZE);
    for (uint32_t i = 0; i < data_block_start; i++)
        bitmap_set(buffer, i);

    if (blkdev_write(bdev, block_bitmap_block, buffer) < 0)
    {
        kfree(buffer);
        return -1;
    }

    // Create root directory inode (inode 0)
    struct vrfs_inode root_inode;
    fs_memset(&root_inode, 0, sizeof(struct vrfs_inode));
    root_inode.mode = VRFS_INODE_DIR;
    root_inode.links_count = 2; // . and ..
    root_inode.size = 0;
    root_inode.blocks = 0;

    // Write root inode
    fs_memset(buffer, 0, VRFS_BLOCK_SIZE);
    fs_memcpy(buffer, &root_inode, sizeof(struct vrfs_inode));
    if (blkdev_write(bdev, inode_table_block, buffer) < 0)
    {
        kfree(buffer);
        return -1;
    }

    kfree(buffer);
    return 0;
}

// Forward declarations
static struct file_operations vrfs_fops;
static struct inode_operations vrfs_iops;

// VRFS operations
static int vrfs_open(struct inode *inode, struct file *file)
{
    (void)file;
    (void)inode;
    return 0;
}

static int vrfs_close(struct file *file)
{
    (void)file;
    return 0;
}

static int vrfs_read(struct file *file, char *buffer, uint32_t size, uint32_t offset)
{
    if (!file || !file->inode || !buffer)
        return -1;

    struct inode *inode = file->inode;
    struct vrfs_inode_info *info = (struct vrfs_inode_info *)inode->private_data;

    if (!info || !inode->sb)
        return -1;

    struct vrfs_sb_info *sbi = (struct vrfs_sb_info *)inode->sb->private_data;
    if (!sbi)
        return -1;

    // Check bounds
    if (offset >= inode->size)
        return 0;

    // Adjust size if reading past end
    if (offset + size > inode->size)
        size = inode->size - offset;

    // Simple implementation: only support reading from offset 0
    if (offset != 0)
        return -1;

    // Check if file has data
    if (info->disk_inode.direct[0] == 0)
        return 0;

    // Read data from first block
    uint8_t *block_buffer = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!block_buffer)
        return -1;

    if (blkdev_read(sbi->bdev, info->disk_inode.direct[0], block_buffer) < 0)
    {
        kfree(block_buffer);
        return -1;
    }

    // Copy data to user buffer
    uint32_t to_read = (size < inode->size) ? size : inode->size;
    fs_memcpy(buffer, block_buffer, to_read);

    kfree(block_buffer);
    return to_read;
}

static int vrfs_write(struct file *file, const char *buffer, uint32_t size, uint32_t offset)
{
    if (!file || !file->inode || !buffer || size == 0)
        return -1;

    struct inode *inode = file->inode;
    struct vrfs_inode_info *info = (struct vrfs_inode_info *)inode->private_data;

    if (!info || !inode->sb)
        return -1;

    struct vrfs_sb_info *sbi = (struct vrfs_sb_info *)inode->sb->private_data;
    if (!sbi)
        return -1;

    // Simple implementation: only support writing from offset 0
    // and size <= one block
    if (offset != 0 || size > VRFS_BLOCK_SIZE)
        return -1;

    // Allocate a data block if needed
    if (info->disk_inode.direct[0] == 0)
    {
        int block_no = vrfs_alloc_block(sbi);
        if (block_no < 0)
            return -1;

        info->disk_inode.direct[0] = block_no;
        info->disk_inode.blocks = 1;
    }

    // Write data to the block
    uint8_t *block_buffer = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!block_buffer)
        return -1;

    fs_memset(block_buffer, 0, VRFS_BLOCK_SIZE);
    fs_memcpy(block_buffer, buffer, size);

    if (blkdev_write(sbi->bdev, info->disk_inode.direct[0], block_buffer) < 0)
    {
        kfree(block_buffer);
        return -1;
    }

    kfree(block_buffer);

    // Update inode size
    info->disk_inode.size = size;
    inode->size = size;

    // Write inode back to disk
    vrfs_write_inode(sbi, info->inode_no, &info->disk_inode);

    return size;
}

// Add directory entry to parent directory
static int vrfs_add_dir_entry(struct vrfs_sb_info *sbi, struct vrfs_inode *dir_inode,
                                  uint32_t dir_inode_no, const char *name, uint32_t inode_no)
{
    // For simplicity: if directory has no data block, allocate one
    if (dir_inode->direct[0] == 0)
    {
        int block_no = vrfs_alloc_block(sbi);
        if (block_no < 0)
            return -1;

        dir_inode->direct[0] = block_no;
        dir_inode->blocks = 1;

        // Write updated directory inode to disk immediately!
        vrfs_write_inode(sbi, dir_inode_no, dir_inode);

        // Clear the block
        uint8_t *block_buf = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
        if (!block_buf)
            return -1;

        fs_memset(block_buf, 0, VRFS_BLOCK_SIZE);
        blkdev_write(sbi->bdev, block_no, block_buf);
        kfree(block_buf);
    }

    // Read directory block
    uint8_t *block_buf = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!block_buf)
        return -1;

    if (blkdev_read(sbi->bdev, dir_inode->direct[0], block_buf) < 0)
    {
        kfree(block_buf);
        return -1;
    }

    // Find first empty entry
    struct vrfs_dirent *entries = (struct vrfs_dirent *)block_buf;
    int max_entries = VRFS_BLOCK_SIZE / sizeof(struct vrfs_dirent);

    for (int i = 0; i < max_entries; i++)
    {
        if (entries[i].inode == 0)
        {
            // Found empty slot
            entries[i].inode = inode_no;

            // Copy name (max 28 chars)
            int j;
            for (j = 0; j < VRFS_MAX_NAME - 1 && name[j]; j++)
                entries[i].name[j] = name[j];
            entries[i].name[j] = '\0';

            // Write back
            if (blkdev_write(sbi->bdev, dir_inode->direct[0], block_buf) < 0)
            {
                kfree(block_buf);
                return -1;
            }

            // Update directory size
            dir_inode->size = (i + 1) * sizeof(struct vrfs_dirent);
            vrfs_write_inode(sbi, dir_inode_no, dir_inode);

            kfree(block_buf);
            return 0;
        }
    }

    kfree(block_buf);
    return -1; // Directory full
}

// Create a new file
static struct inode *vrfs_create(struct inode *dir, const char *name, uint32_t mode)
{
    if (!dir || !name || !dir->sb)
        return 0;

    struct vrfs_sb_info *sbi = (struct vrfs_sb_info *)dir->sb->private_data;
    if (!sbi)
        return 0;

    (void)mode;

    // Get parent directory inode info
    struct vrfs_inode_info *dir_info = (struct vrfs_inode_info *)dir->private_data;
    if (!dir_info)
        return 0;

    // Check if file already exists using lookup
    if (dir->i_op && dir->i_op->lookup)
    {
        struct inode *existing = dir->i_op->lookup(dir, name);
        if (existing)
        {
            // File already exists, return the existing inode
            return existing;
        }
    }

    // Allocate new inode number
    int inode_no = vrfs_alloc_inode(sbi);
    if (inode_no < 0)
        return 0;

    // Allocate VFS inode
    struct inode *new_inode = (struct inode *)kmalloc(sizeof(struct inode));
    if (!new_inode)
        return 0;

    struct vrfs_inode_info *info = (struct vrfs_inode_info *)kmalloc(sizeof(struct vrfs_inode_info));
    if (!info)
    {
        kfree(new_inode);
        return 0;
    }

    // Initialize disk inode
    fs_memset(&info->disk_inode, 0, sizeof(struct vrfs_inode));
    info->disk_inode.mode = VRFS_INODE_FILE;
    info->disk_inode.links_count = 1;
    info->disk_inode.size = 0;
    info->disk_inode.blocks = 0;
    info->inode_no = inode_no;

    // Write inode to disk
    vrfs_write_inode(sbi, inode_no, &info->disk_inode);

    // Add directory entry to parent
    int add_result = vrfs_add_dir_entry(sbi, &dir_info->disk_inode, dir_info->inode_no, name, inode_no);

    if (add_result < 0)
    {
        // Failed to add directory entry, cleanup
        kfree(info);
        kfree(new_inode);
        return 0;
    }

    // Initialize VFS inode
    new_inode->ino = inode_no;
    new_inode->mode = VFS_FILE;
    new_inode->type = VFS_FILE;
    new_inode->size = 0;
    new_inode->f_op = &vrfs_fops;
    new_inode->i_op = &vrfs_iops;
    new_inode->sb = dir->sb;
    new_inode->private_data = info;

    return new_inode;
}

// Lookup a file in directory
static struct inode *vrfs_lookup(struct inode *dir, const char *name)
{
    if (!dir || !name || !dir->sb)
        return 0;

    struct vrfs_sb_info *sbi = (struct vrfs_sb_info *)dir->sb->private_data;
    if (!sbi)
        return 0;

    struct vrfs_inode_info *dir_info = (struct vrfs_inode_info *)dir->private_data;
    if (!dir_info)
        return 0;

    // Check if directory has any data blocks
    if (dir_info->disk_inode.direct[0] == 0)
        return 0;

    // Read directory block
    uint8_t *block_buf = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!block_buf)
        return 0;

    if (blkdev_read(sbi->bdev, dir_info->disk_inode.direct[0], block_buf) < 0)
    {
        kfree(block_buf);
        return 0;
    }

    // Search for name
    struct vrfs_dirent *entries = (struct vrfs_dirent *)block_buf;
    int max_entries = VRFS_BLOCK_SIZE / sizeof(struct vrfs_dirent);

    for (int i = 0; i < max_entries; i++)
    {
        if (entries[i].inode == 0)
            continue;

        // Compare names
        int match = 1;
        for (int j = 0; j < VRFS_MAX_NAME; j++)
        {
            if (entries[i].name[j] != name[j])
            {
                match = 0;
                break;
            }
            if (entries[i].name[j] == '\0')
                break;
        }

        if (match)
        {
            // Found! Load the inode
            uint32_t inode_no = entries[i].inode;
            kfree(block_buf);

            // Read inode from disk
            struct vrfs_inode disk_inode;
            if (vrfs_read_inode(sbi, inode_no, &disk_inode) < 0)
                return 0;

            // Create VFS inode
            struct inode *found_inode = (struct inode *)kmalloc(sizeof(struct inode));
            if (!found_inode)
                return 0;

            struct vrfs_inode_info *info = (struct vrfs_inode_info *)kmalloc(sizeof(struct vrfs_inode_info));
            if (!info)
            {
                kfree(found_inode);
                return 0;
            }

            info->disk_inode = disk_inode;
            info->inode_no = inode_no;

            found_inode->ino = inode_no;
            found_inode->mode = (disk_inode.mode == VRFS_INODE_DIR) ? VFS_DIRECTORY : VFS_FILE;
            found_inode->type = found_inode->mode;
            found_inode->size = disk_inode.size;
            found_inode->f_op = &vrfs_fops;
            found_inode->i_op = &vrfs_iops;
            found_inode->sb = dir->sb;
            found_inode->private_data = info;

            return found_inode;
        }
    }

    kfree(block_buf);
    return 0; // Not found
}

// Initialize operations structures
static struct file_operations vrfs_fops = {
    .open = vrfs_open,
    .close = vrfs_close,
    .read = vrfs_read,
    .write = vrfs_write,
};

static struct inode_operations vrfs_iops = {
    .create = vrfs_create,
    .lookup = vrfs_lookup,
    .unlink = 0,
    .mkdir = 0,
    .rmdir = 0,
};

// Mount VRFS filesystem
struct superblock *vrfs_mount(struct block_device *bdev)
{
    if (!bdev)
        return 0;

    // Read superblock
    uint8_t *buffer = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!buffer)
        return 0;

    if (blkdev_read(bdev, 0, buffer) < 0)
    {
        kfree(buffer);
        return 0;
    }

    struct vrfs_superblock *sb_disk = (struct vrfs_superblock *)buffer;

    // Verify magic
    if (sb_disk->magic != VRFS_MAGIC)
    {
        kfree(buffer);
        return 0;
    }

    // Allocate superblock info
    struct vrfs_sb_info *sbi = (struct vrfs_sb_info *)kmalloc(sizeof(struct vrfs_sb_info));
    if (!sbi)
    {
        kfree(buffer);
        return 0;
    }

    // Copy superblock
    fs_memcpy(&sbi->sb, sb_disk, sizeof(struct vrfs_superblock));
    sbi->bdev = bdev;

    // Allocate and read inode bitmap
    sbi->inode_bitmap = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!sbi->inode_bitmap)
    {
        kfree(sbi);
        kfree(buffer);
        return 0;
    }
    blkdev_read(bdev, sbi->sb.inode_bitmap_block, sbi->inode_bitmap);

    // Allocate and read block bitmap
    sbi->block_bitmap = (uint8_t *)kmalloc(VRFS_BLOCK_SIZE);
    if (!sbi->block_bitmap)
    {
        kfree(sbi->inode_bitmap);
        kfree(sbi);
        kfree(buffer);
        return 0;
    }
    blkdev_read(bdev, sbi->sb.block_bitmap_block, sbi->block_bitmap);

    // Create VFS superblock
    struct superblock *vfs_sb = (struct superblock *)kmalloc(sizeof(struct superblock));
    if (!vfs_sb)
    {
        kfree(sbi->block_bitmap);
        kfree(sbi->inode_bitmap);
        kfree(sbi);
        kfree(buffer);
        return 0;
    }

    vfs_sb->magic = VRFS_MAGIC;
    vfs_sb->block_size = VRFS_BLOCK_SIZE;
    vfs_sb->private_data = sbi;

    // Create root inode
    struct inode *root_inode = (struct inode *)kmalloc(sizeof(struct inode));
    if (!root_inode)
    {
        kfree(vfs_sb);
        kfree(sbi->block_bitmap);
        kfree(sbi->inode_bitmap);
        kfree(sbi);
        kfree(buffer);
        return 0;
    }

    // Read root inode (inode 0) from disk
    struct vrfs_inode_info *root_info = (struct vrfs_inode_info *)kmalloc(sizeof(struct vrfs_inode_info));
    if (!root_info)
    {
        kfree(root_inode);
        kfree(vfs_sb);
        kfree(sbi->block_bitmap);
        kfree(sbi->inode_bitmap);
        kfree(sbi);
        kfree(buffer);
        return 0;
    }

    if (vrfs_read_inode(sbi, 0, &root_info->disk_inode) < 0)
    {
        kfree(root_info);
        kfree(root_inode);
        kfree(vfs_sb);
        kfree(sbi->block_bitmap);
        kfree(sbi->inode_bitmap);
        kfree(sbi);
        kfree(buffer);
        return 0;
    }

    root_info->inode_no = 0;

    root_inode->ino = 0;
    root_inode->mode = VFS_DIRECTORY;
    root_inode->type = VFS_DIRECTORY;
    root_inode->size = root_info->disk_inode.size;
    root_inode->f_op = &vrfs_fops;
    root_inode->i_op = &vrfs_iops;
    root_inode->sb = vfs_sb;
    root_inode->private_data = root_info;

    vfs_sb->root_inode = root_inode;

    kfree(buffer);
    return vfs_sb;
}

// Unmount VRFS
int vrfs_unmount(struct superblock *sb)
{
    if (!sb)
        return -1;

    struct vrfs_sb_info *sbi = (struct vrfs_sb_info *)sb->private_data;
    if (sbi)
    {
        if (sbi->block_bitmap)
            kfree(sbi->block_bitmap);
        if (sbi->inode_bitmap)
            kfree(sbi->inode_bitmap);
        kfree(sbi);
    }

    if (sb->root_inode)
        kfree(sb->root_inode);

    kfree(sb);
    return 0;
}

// Initialize VRFS
int vrfs_init(void)
{
    // Nothing to do for now
    return 0;
}
