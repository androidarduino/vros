#include "ramfs.h"
#include "kmalloc.h"

// Forward declarations
static struct file_operations ramfs_fops;
static struct inode_operations ramfs_iops;

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

// Helper: string compare
static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return *s1 - *s2;
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

// Add directory entry
static int ramfs_add_entry(struct ramfs_node *dir_node, const char *name, struct inode *inode)
{
    if (!dir_node || !name || !inode)
        return -1;

    // Allocate new entry
    struct ramfs_dirent *entry = (struct ramfs_dirent *)kmalloc(sizeof(struct ramfs_dirent));
    if (!entry)
        return -1;

    // Initialize entry
    strcpy(entry->name, name);
    entry->inode = inode;
    entry->next = dir_node->entries;

    // Add to list
    dir_node->entries = entry;
    dir_node->num_entries++;

    return 0;
}

// Remove directory entry
static int ramfs_remove_entry(struct ramfs_node *dir_node, const char *name)
{
    if (!dir_node || !name)
        return -1;

    struct ramfs_dirent *prev = 0;
    struct ramfs_dirent *curr = dir_node->entries;

    while (curr)
    {
        if (strcmp(curr->name, name) == 0)
        {
            // Found entry, remove it
            if (prev)
                prev->next = curr->next;
            else
                dir_node->entries = curr->next;

            kfree(curr);
            dir_node->num_entries--;
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1; // Not found
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
    if (required > node->size)
    {
        node->size = required;
        file->inode->size = node->size;
    }

    return size;
}

// File operations table
static struct file_operations ramfs_fops = {
    .open = ramfs_open,
    .close = ramfs_close,
    .read = ramfs_read,
    .write = ramfs_write};

// Directory operations
static int ramfs_readdir(struct file *file, struct dentry *dentry, uint32_t index)
{
    struct ramfs_node *node = (struct ramfs_node *)file->inode->private_data;
    if (!node || file->inode->type != VFS_DIRECTORY)
        return -1;

    // Traverse to find entry at index
    struct ramfs_dirent *entry = node->entries;
    uint32_t i = 0;
    while (entry && i < index)
    {
        entry = entry->next;
        i++;
    }

    if (!entry)
        return -1; // No more entries

    // Fill dentry
    strcpy(dentry->name, entry->name);
    dentry->inode = entry->inode;

    return 0;
}

// ramfs inode operations
static struct inode *ramfs_create(struct inode *dir, const char *name, uint32_t mode)
{
    (void)mode;

    if (!dir || dir->type != VFS_DIRECTORY)
        return 0;

    // Check if entry already exists
    struct ramfs_node *dir_node = (struct ramfs_node *)dir->private_data;
    struct ramfs_dirent *entry = dir_node->entries;
    while (entry)
    {
        if (strcmp(entry->name, name) == 0)
            return 0; // Already exists
        entry = entry->next;
    }

    // Allocate new inode
    struct inode *inode = vfs_alloc_inode(dir->sb);
    if (!inode)
        return 0;

    inode->type = VFS_FILE;
    inode->f_op = &ramfs_fops;
    inode->i_op = &ramfs_iops;

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

    // Add to parent directory
    if (ramfs_add_entry(dir_node, name, inode) < 0)
    {
        kfree(node);
        vfs_free_inode(inode);
        return 0;
    }

    return inode;
}

static struct inode *ramfs_lookup(struct inode *dir, const char *name)
{
    if (!dir || dir->type != VFS_DIRECTORY)
        return 0;

    struct ramfs_node *node = (struct ramfs_node *)dir->private_data;
    if (!node)
        return 0;

    // Search for entry
    struct ramfs_dirent *entry = node->entries;
    while (entry)
    {
        if (strcmp(entry->name, name) == 0)
            return entry->inode;
        entry = entry->next;
    }

    return 0; // Not found
}

static int ramfs_unlink(struct inode *dir, const char *name)
{
    if (!dir || dir->type != VFS_DIRECTORY)
        return -1;

    struct ramfs_node *dir_node = (struct ramfs_node *)dir->private_data;
    if (!dir_node)
        return -1;

    // Find and remove entry
    struct ramfs_dirent *prev = 0;
    struct ramfs_dirent *curr = dir_node->entries;

    while (curr)
    {
        if (strcmp(curr->name, name) == 0)
        {
            // Check if it's a directory
            if (curr->inode->type == VFS_DIRECTORY)
                return -1; // Use rmdir for directories

            // Remove from list
            if (prev)
                prev->next = curr->next;
            else
                dir_node->entries = curr->next;

            // Free resources
            struct ramfs_node *node = (struct ramfs_node *)curr->inode->private_data;
            if (node)
            {
                if (node->data)
                    kfree(node->data);
                kfree(node);
            }

            vfs_free_inode(curr->inode);
            kfree(curr);
            dir_node->num_entries--;

            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1; // Not found
}

static struct inode *ramfs_mkdir(struct inode *dir, const char *name, uint32_t mode)
{
    (void)mode;

    if (!dir || dir->type != VFS_DIRECTORY)
        return 0;

    // Check if entry already exists
    struct ramfs_node *dir_node = (struct ramfs_node *)dir->private_data;
    struct ramfs_dirent *entry = dir_node->entries;
    while (entry)
    {
        if (strcmp(entry->name, name) == 0)
            return 0; // Already exists
        entry = entry->next;
    }

    // Allocate new inode
    struct inode *inode = vfs_alloc_inode(dir->sb);
    if (!inode)
        return 0;

    inode->type = VFS_DIRECTORY;
    inode->f_op = &ramfs_fops;
    inode->i_op = &ramfs_iops;

    // Allocate ramfs node for directory
    struct ramfs_node *node = (struct ramfs_node *)kmalloc(sizeof(struct ramfs_node));
    if (!node)
    {
        vfs_free_inode(inode);
        return 0;
    }

    node->entries = 0;
    node->num_entries = 0;

    inode->private_data = node;

    // Add to parent directory
    if (ramfs_add_entry(dir_node, name, inode) < 0)
    {
        kfree(node);
        vfs_free_inode(inode);
        return 0;
    }

    return inode;
}

static int ramfs_rmdir(struct inode *dir, const char *name)
{
    if (!dir || dir->type != VFS_DIRECTORY)
        return -1;

    struct ramfs_node *dir_node = (struct ramfs_node *)dir->private_data;
    if (!dir_node)
        return -1;

    // Find directory entry
    struct ramfs_dirent *prev = 0;
    struct ramfs_dirent *curr = dir_node->entries;

    while (curr)
    {
        if (strcmp(curr->name, name) == 0)
        {
            // Check if it's a directory
            if (curr->inode->type != VFS_DIRECTORY)
                return -1; // Not a directory

            // Check if directory is empty
            struct ramfs_node *target_node = (struct ramfs_node *)curr->inode->private_data;
            if (target_node && target_node->num_entries > 0)
                return -1; // Directory not empty

            // Remove from list
            if (prev)
                prev->next = curr->next;
            else
                dir_node->entries = curr->next;

            // Free resources
            if (target_node)
                kfree(target_node);

            vfs_free_inode(curr->inode);
            kfree(curr);
            dir_node->num_entries--;

            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1; // Not found
}

// ramfs inode operations table
static struct inode_operations ramfs_iops = {
    .create = ramfs_create,
    .lookup = ramfs_lookup,
    .unlink = ramfs_unlink,
    .mkdir = ramfs_mkdir,
    .rmdir = ramfs_rmdir};

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
    if (!root_sb)
        return -1;

    // Create root inode
    struct inode *root_inode = vfs_alloc_inode(root_sb);
    if (!root_inode)
        return -1;

    root_inode->type = VFS_DIRECTORY;
    root_inode->i_op = &ramfs_iops;
    root_inode->f_op = &ramfs_fops;

    // Allocate directory node
    struct ramfs_node *root_node = (struct ramfs_node *)kmalloc(sizeof(struct ramfs_node));
    if (!root_node)
    {
        vfs_free_inode(root_inode);
        return -1;
    }

    root_node->entries = 0;
    root_node->num_entries = 0;
    root_inode->private_data = root_node;

    root_sb->root_inode = root_inode;

    return 0;
}

// Helper: parse path and find parent directory
static struct inode *ramfs_find_parent(const char *path, char *filename)
{
    struct superblock *root_sb = vfs_get_root_sb();
    if (!root_sb || !root_sb->root_inode)
        return 0;

    // Start from root
    struct inode *current = root_sb->root_inode;

    // Skip leading slash
    if (*path == '/')
        path++;

    // If empty path, use root
    if (*path == '\0')
    {
        strcpy(filename, "");
        return current;
    }

    // Parse path
    char component[VFS_NAME_MAX];
    int comp_len = 0;

    while (*path)
    {
        // Extract component
        comp_len = 0;
        while (*path && *path != '/' && comp_len < VFS_NAME_MAX - 1)
        {
            component[comp_len++] = *path++;
        }
        component[comp_len] = '\0';

        // Skip slash
        if (*path == '/')
            path++;

        // If this is the last component, it's the filename
        if (*path == '\0')
        {
            strcpy(filename, component);
            return current;
        }

        // Otherwise, lookup this component
        struct inode *next = ramfs_lookup(current, component);
        if (!next || next->type != VFS_DIRECTORY)
            return 0; // Path not found or not a directory

        current = next;
    }

    strcpy(filename, "");
    return current;
}

// Create a file in ramfs (supports paths)
struct inode *ramfs_create_file(const char *path, const char *initial_content)
{
    char filename[VFS_NAME_MAX];
    struct inode *parent = ramfs_find_parent(path, filename);

    if (!parent || *filename == '\0')
        return 0;

    // Create inode
    struct inode *inode = ramfs_create(parent, filename, 0644);
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

// Create a directory in ramfs (supports paths)
struct inode *ramfs_create_dir(const char *path, uint32_t mode)
{
    char dirname[VFS_NAME_MAX];
    struct inode *parent = ramfs_find_parent(path, dirname);

    if (!parent || *dirname == '\0')
        return 0;

    return ramfs_mkdir(parent, dirname, mode);
}
