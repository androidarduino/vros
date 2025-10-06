#ifndef RAMFS_H
#define RAMFS_H

#include "vfs.h"

// RAM filesystem directory entry
struct ramfs_dirent
{
    char name[VFS_NAME_MAX];   // Entry name
    struct inode *inode;       // Associated inode
    struct ramfs_dirent *next; // Next entry in directory
};

// RAM filesystem node
struct ramfs_node
{
    union
    {
        // For files
        struct
        {
            char *data;        // File data
            uint32_t capacity; // Allocated capacity
            uint32_t size;     // Actual data size
        };
        // For directories
        struct
        {
            struct ramfs_dirent *entries; // Directory entries
            uint32_t num_entries;         // Number of entries
        };
    };
};

// Initialize ramfs
void ramfs_init(void);

// Mount ramfs as root
int ramfs_mount_root(void);

// Create a file in ramfs
struct inode *ramfs_create_file(const char *path, const char *initial_content);

// Create a directory
struct inode *ramfs_create_dir(const char *path, uint32_t mode);

#endif // RAMFS_H
