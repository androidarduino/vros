#include "ramfs.h"
#include "kmalloc.h"

// Helper: string copy
static void strcpy(char *dest, const char *src)
{
    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Helper: string length
static int strlen(const char *s)
{
    int len = 0;
    while (s[len])
        len++;
    return len;
}

// Helper: memory copy
static void memcpy(void *dest, const void *src, uint32_t n)
{
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (n--)
    {
        *d++ = *s++;
    }
}

// ramfs file operations
static int ramfs_open(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

static int ramfs_close(struct file *file)
{
    (void)file;
    return 0;
}

static int ramfs_read(struct file *file, char *buffer, uint32_t size, uint32_t offset)
{
    struct ramfs_node *node = (struct ramfs_node *)file->inode->private_data;
    if (!node || !node->data)
        return 0;

    // Check bounds
    if (offset >= node->size)
        return 0;

    // Calculate actual read size
    uint32_t to_read = size;
    if (offset + to_read > node->size)
    {
        to_read = node->size - offset;
    }

    // Copy data
    memcpy(buffer, node->data + offset, to_read);

    return to_read;
}

static int ramfs_write(struct file *file, const char *buffer, uint32_t size, uint32_t offset)
{
    struct ramfs_node *node = (struct ramfs_node *)file->inode->private_data;
    if (!node)
        return -1;

    // Calculate required capacity
    uint32_t required = offset + size;

    // Expand if needed
    if (required > node->capacity)
    {
        uint32_t new_capacity = required * 2;
        char *new_data = (char *)kmalloc(new_capacity);
        if (!new_data)
            return -1;

        // Copy old data
        if (node->data)
        {
            memcpy(new_data, node->data, node->size);
            kfree(node->data);
        }

        node->data = new_data;
        node->capacity = new_capacity;
    }

    // Write data
    memcpy(node->data + offset, buffer, size);

    // Update size
    if (offset + size > node->size)
    {
        node->size = offset + size;
        file->inode->size = node->size;
    }

    return size;
}

// ramfs file operations table
static struct file_operations ramfs_fops = {
    .open = ramfs_open,
    .close = ramfs_close,
    .read = ramfs_read,
    .write = ramfs_write,
    .lseek = 0,
    .readdir = 0};

// ramfs inode operations
static struct inode *ramfs_create(struct inode *dir, const char *name, uint32_t mode)
{
    (void)mode;

    if (!dir || dir->type != VFS_DIRECTORY)
        return 0;

    // Allocate new inode
    struct inode *inode = vfs_alloc_inode(dir->sb);
    if (!inode)
        return 0;

    inode->type = VFS_FILE;
    inode->f_op = &ramfs_fops;

    // Allocate ramfs node
    struct ramfs_node *node = (struct ramfs_node *)kmalloc(sizeof(struct ramfs_node));
    if (!node)
    {
        vfs_free_inode(inode);
        return 0;
    }

    node->data = 0;
    node->capacity = 0;
    node->size = 0;
    inode->private_data = node;

    // Create dentry
    struct dentry *dentry = vfs_alloc_dentry(name, inode);
    if (!dentry)
    {
        kfree(node);
        vfs_free_inode(inode);
        return 0;
    }

    // Get parent dentry (need to find it from root)
    extern struct dentry *vfs_lookup(const char *path);
    struct dentry *parent_dentry = vfs_lookup("/");
    if (!parent_dentry)
    {
        vfs_free_dentry(dentry);
        kfree(node);
        vfs_free_inode(inode);
        return 0;
    }

    // Add to parent's children
    dentry->parent = parent_dentry;
    dentry->next = parent_dentry->child;
    parent_dentry->child = dentry;

    return inode;
}

static struct inode *ramfs_lookup(struct inode *dir, const char *name)
{
    (void)dir;
    (void)name;
    return 0;
}

// ramfs inode operations table
static struct inode_operations ramfs_iops = {
    .create = ramfs_create,
    .lookup = ramfs_lookup,
    .unlink = 0,
    .mkdir = 0,
    .rmdir = 0};

// Initialize ramfs
void ramfs_init(void)
{
    // Nothing to initialize here
}

// Mount ramfs as root
int ramfs_mount_root(void)
{
    // VFS should already be initialized
    struct superblock *root_sb = vfs_get_root_sb();

    // Set operations on root inode
    if (root_sb && root_sb->root_inode)
    {
        root_sb->root_inode->i_op = &ramfs_iops;
    }

    return 0;
}

// Create a file in ramfs with initial content
struct inode *ramfs_create_file(const char *path, const char *initial_content)
{
    (void)path;

    // For simplicity, create in root directory
    struct superblock *root_sb = vfs_get_root_sb();
    if (!root_sb || !root_sb->root_inode)
        return 0;

    // Extract filename from path (simple version - assume no directory)
    const char *filename = path;
    if (*filename == '/')
        filename++;

    // Create inode
    struct inode *inode = ramfs_create(root_sb->root_inode, filename, 0644);
    if (!inode)
        return 0;

    // Write initial content if provided
    if (initial_content)
    {
        struct ramfs_node *node = (struct ramfs_node *)inode->private_data;
        int len = strlen(initial_content);

        node->data = (char *)kmalloc(len + 1);
        if (node->data)
        {
            strcpy(node->data, initial_content);
            node->size = len;
            node->capacity = len + 1;
            inode->size = len;
        }
    }

    return inode;
}
