#include "devfs.h"
#include "kmalloc.h"

// Random seed for /dev/random
static uint32_t random_seed = 12345;

// Helper: string copy
static void strcpy(char *dest, const char *src)
{
    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Simple pseudo-random number generator (Linear Congruential Generator)
static uint32_t rand_next(void)
{
    random_seed = random_seed * 1103515245 + 12345;
    return (random_seed / 65536) % 32768;
}

// /dev/null operations
static int dev_null_read(void *private_data, char *buffer, uint32_t size, uint32_t offset)
{
    (void)private_data;
    (void)buffer;
    (void)size;
    (void)offset;
    return 0; // Always return 0 bytes (EOF)
}

static int dev_null_write(void *private_data, const char *buffer, uint32_t size, uint32_t offset)
{
    (void)private_data;
    (void)buffer;
    (void)offset;
    return size; // Pretend to write everything, but discard it
}

static struct device_operations dev_null_ops = {
    .read = dev_null_read,
    .write = dev_null_write};

// /dev/zero operations
static int dev_zero_read(void *private_data, char *buffer, uint32_t size, uint32_t offset)
{
    (void)private_data;
    (void)offset;

    // Fill buffer with zeros
    for (uint32_t i = 0; i < size; i++)
    {
        buffer[i] = 0;
    }

    return size;
}

static int dev_zero_write(void *private_data, const char *buffer, uint32_t size, uint32_t offset)
{
    (void)private_data;
    (void)buffer;
    (void)offset;
    return size; // Accept writes but do nothing
}

static struct device_operations dev_zero_ops = {
    .read = dev_zero_read,
    .write = dev_zero_write};

// /dev/random operations
static int dev_random_read(void *private_data, char *buffer, uint32_t size, uint32_t offset)
{
    (void)private_data;
    (void)offset;

    // Fill buffer with pseudo-random bytes
    for (uint32_t i = 0; i < size; i++)
    {
        buffer[i] = (char)(rand_next() & 0xFF);
    }

    return size;
}

static int dev_random_write(void *private_data, const char *buffer, uint32_t size, uint32_t offset)
{
    (void)private_data;
    (void)offset;

    // Use written data to update random seed
    for (uint32_t i = 0; i < size; i++)
    {
        random_seed ^= buffer[i];
        random_seed = (random_seed << 1) | (random_seed >> 31);
    }

    return size;
}

static struct device_operations dev_random_ops = {
    .read = dev_random_read,
    .write = dev_random_write};

// devfs file operations
static int devfs_open(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

static int devfs_close(struct file *file)
{
    (void)file;
    return 0;
}

static int devfs_read(struct file *file, char *buffer, uint32_t size, uint32_t offset)
{
    struct devfs_node *node = (struct devfs_node *)file->inode->private_data;
    if (!node)
        return 0;

    // Call appropriate device read operation
    switch (node->type)
    {
    case DEV_NULL:
        return dev_null_ops.read(node->private_data, buffer, size, offset);
    case DEV_ZERO:
        return dev_zero_ops.read(node->private_data, buffer, size, offset);
    case DEV_RANDOM:
        return dev_random_ops.read(node->private_data, buffer, size, offset);
    default:
        return 0;
    }
}

static int devfs_write(struct file *file, const char *buffer, uint32_t size, uint32_t offset)
{
    struct devfs_node *node = (struct devfs_node *)file->inode->private_data;
    if (!node)
        return -1;

    // Call appropriate device write operation
    switch (node->type)
    {
    case DEV_NULL:
        return dev_null_ops.write(node->private_data, buffer, size, offset);
    case DEV_ZERO:
        return dev_zero_ops.write(node->private_data, buffer, size, offset);
    case DEV_RANDOM:
        return dev_random_ops.write(node->private_data, buffer, size, offset);
    default:
        return -1;
    }
}

// devfs file operations table
static struct file_operations devfs_fops = {
    .open = devfs_open,
    .close = devfs_close,
    .read = devfs_read,
    .write = devfs_write,
    .lseek = 0,
    .readdir = 0};

// Register a device
int devfs_register_device(const char *name, devfs_device_type_t type, struct device_operations *ops)
{
    (void)ops; // Will use device type to determine operations

    struct superblock *root_sb = vfs_get_root_sb();
    if (!root_sb)
        return -1;

    // Find /dev directory
    struct dentry *dev_dir = vfs_lookup("/dev");
    if (!dev_dir)
        return -1;

    // Allocate new inode
    struct inode *inode = vfs_alloc_inode(root_sb);
    if (!inode)
        return -1;

    inode->type = VFS_CHARDEVICE;
    inode->mode = 0666; // Read/write for all
    inode->f_op = &devfs_fops;

    // Allocate devfs node
    struct devfs_node *node = (struct devfs_node *)kmalloc(sizeof(struct devfs_node));
    if (!node)
    {
        vfs_free_inode(inode);
        return -1;
    }

    node->type = type;
    node->private_data = 0;
    inode->private_data = node;

    // Create dentry
    struct dentry *dentry = vfs_alloc_dentry(name, inode);
    if (!dentry)
    {
        kfree(node);
        vfs_free_inode(inode);
        return -1;
    }

    // Add to /dev's children
    dentry->parent = dev_dir;
    dentry->next = dev_dir->child;
    dev_dir->child = dentry;

    return 0;
}

// Initialize devfs
void devfs_init(void)
{
    // Nothing to initialize here
}

// Mount devfs at /dev
int devfs_mount(void)
{
    struct superblock *root_sb = vfs_get_root_sb();
    if (!root_sb || !root_sb->root)
        return -1;

    // Create /dev directory
    struct inode *dev_inode = vfs_alloc_inode(root_sb);
    if (!dev_inode)
        return -1;

    dev_inode->type = VFS_DIRECTORY;
    dev_inode->mode = 0755; // Read/execute for all, write for owner

    struct dentry *dev_dentry = vfs_alloc_dentry("dev", dev_inode);
    if (!dev_dentry)
    {
        vfs_free_inode(dev_inode);
        return -1;
    }

    // Add to root's children
    dev_dentry->parent = root_sb->root;
    dev_dentry->next = root_sb->root->child;
    root_sb->root->child = dev_dentry;

    // Register standard devices
    devfs_register_device("null", DEV_NULL, &dev_null_ops);
    devfs_register_device("zero", DEV_ZERO, &dev_zero_ops);
    devfs_register_device("random", DEV_RANDOM, &dev_random_ops);

    return 0;
}
