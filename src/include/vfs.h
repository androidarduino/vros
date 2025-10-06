#ifndef VFS_H
#define VFS_H

#include <stdint.h>

// File types
#define VFS_FILE 0x01
#define VFS_DIRECTORY 0x02
#define VFS_CHARDEVICE 0x03
#define VFS_BLOCKDEVICE 0x04
#define VFS_PIPE 0x05
#define VFS_SYMLINK 0x06
#define VFS_MOUNTPOINT 0x08

// Maximum file name length
#define VFS_NAME_MAX 256

// Maximum open files per process
#define VFS_MAX_OPEN_FILES 16

// Forward declarations
struct inode;
struct file;
struct dentry;
struct superblock;
struct file_operations;
struct inode_operations;

// Inode - represents a file system object
struct inode
{
    uint32_t ino;                  // Inode number
    uint32_t mode;                 // File mode (permissions)
    uint32_t type;                 // File type
    uint32_t uid;                  // Owner user ID
    uint32_t gid;                  // Owner group ID
    uint32_t size;                 // File size in bytes
    uint32_t atime;                // Last access time
    uint32_t mtime;                // Last modification time
    uint32_t ctime;                // Last status change time
    uint32_t links;                // Number of hard links
    void *private_data;            // Private data for fs implementation
    struct inode_operations *i_op; // Inode operations
    struct file_operations *f_op;  // File operations
    struct superblock *sb;         // Superblock
};

// File - represents an open file
struct file
{
    struct inode *inode;          // Associated inode
    uint32_t flags;               // File flags (O_RDONLY, etc.)
    uint32_t pos;                 // Current file position
    uint32_t ref_count;           // Reference count
    struct file_operations *f_op; // File operations
    void *private_data;           // Private data
};

// Directory entry
struct dentry
{
    char name[VFS_NAME_MAX]; // File name
    struct inode *inode;     // Associated inode
    struct dentry *parent;   // Parent directory
    struct dentry *next;     // Next sibling
    struct dentry *child;    // First child (if directory)
};

// Superblock - file system metadata
struct superblock
{
    uint32_t magic;           // Magic number
    uint32_t block_size;      // Block size
    uint32_t max_files;       // Maximum number of files
    void *private_data;       // Private data for fs implementation
    struct dentry *root;      // Root directory entry
    struct inode *root_inode; // Root inode
};

// File operations
struct file_operations
{
    int (*open)(struct inode *inode, struct file *file);
    int (*close)(struct file *file);
    int (*read)(struct file *file, char *buffer, uint32_t size, uint32_t offset);
    int (*write)(struct file *file, const char *buffer, uint32_t size, uint32_t offset);
    int (*lseek)(struct file *file, uint32_t offset, int whence);
    int (*readdir)(struct file *file, struct dentry *dentry);
};

// Inode operations
struct inode_operations
{
    struct inode *(*create)(struct inode *dir, const char *name, uint32_t mode);
    struct inode *(*lookup)(struct inode *dir, const char *name);
    int (*unlink)(struct inode *dir, const char *name);
    int (*mkdir)(struct inode *dir, const char *name, uint32_t mode);
    int (*rmdir)(struct inode *dir, const char *name);
};

// VFS function declarations
void vfs_init(void);
struct file *vfs_open(const char *path, uint32_t flags);
int vfs_close(struct file *file);
int vfs_read(struct file *file, char *buffer, uint32_t size);
int vfs_write(struct file *file, const char *buffer, uint32_t size);
int vfs_lseek(struct file *file, uint32_t offset, int whence);
struct inode *vfs_create(const char *path, uint32_t mode);
int vfs_unlink(const char *path);
int vfs_mkdir(const char *path, uint32_t mode);
int vfs_rmdir(const char *path);
struct dentry *vfs_lookup(const char *path);

// Helper functions
struct inode *vfs_alloc_inode(struct superblock *sb);
void vfs_free_inode(struct inode *inode);
struct dentry *vfs_alloc_dentry(const char *name, struct inode *inode);
void vfs_free_dentry(struct dentry *dentry);
struct superblock *vfs_get_root_sb(void);
struct inode *vfs_lookup_inode(const char *path);

#endif // VFS_H
