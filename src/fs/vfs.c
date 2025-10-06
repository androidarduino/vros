#include "vfs.h"
#include "kmalloc.h"

// Root superblock
static struct superblock *root_sb = 0;

// Next inode number
static uint32_t next_ino = 1;

// Helper: string comparison
static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

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

// Helper: split path into components
static int split_path(const char *path, char components[][VFS_NAME_MAX], int max_components)
{
    int count = 0;
    int comp_idx = 0;

    // Skip leading slashes
    while (*path == '/')
        path++;

    while (*path && count < max_components)
    {
        if (*path == '/')
        {
            components[count][comp_idx] = '\0';
            count++;
            comp_idx = 0;
            path++;
        }
        else
        {
            components[count][comp_idx++] = *path++;
            if (comp_idx >= VFS_NAME_MAX - 1)
            {
                return -1; // Path component too long
            }
        }
    }

    if (comp_idx > 0)
    {
        components[count][comp_idx] = '\0';
        count++;
    }

    return count;
}

// Allocate an inode
struct inode *vfs_alloc_inode(struct superblock *sb)
{
    struct inode *inode = (struct inode *)kmalloc(sizeof(struct inode));
    if (!inode)
        return 0;

    inode->ino = next_ino++;
    inode->mode = 0755;
    inode->type = VFS_FILE;
    inode->uid = 0;
    inode->gid = 0;
    inode->size = 0;
    inode->atime = 0;
    inode->mtime = 0;
    inode->ctime = 0;
    inode->links = 1;
    inode->private_data = 0;
    inode->i_op = 0;
    inode->f_op = 0;
    inode->sb = sb;

    return inode;
}

// Free an inode
void vfs_free_inode(struct inode *inode)
{
    if (inode)
    {
        kfree(inode);
    }
}

// Allocate a dentry
struct dentry *vfs_alloc_dentry(const char *name, struct inode *inode)
{
    struct dentry *dentry = (struct dentry *)kmalloc(sizeof(struct dentry));
    if (!dentry)
        return 0;

    strcpy(dentry->name, name);
    dentry->inode = inode;
    dentry->parent = 0;
    dentry->next = 0;
    dentry->child = 0;

    return dentry;
}

// Free a dentry
void vfs_free_dentry(struct dentry *dentry)
{
    if (dentry)
    {
        kfree(dentry);
    }
}

// Lookup a path and return the dentry
struct dentry *vfs_lookup(const char *path)
{
    if (!root_sb || !root_sb->root)
        return 0;

    // Handle root directory
    if (strcmp(path, "/") == 0 || strlen(path) == 0)
    {
        return root_sb->root;
    }

    // Split path into components
    char components[16][VFS_NAME_MAX];
    int count = split_path(path, components, 16);
    if (count < 0)
        return 0;

    // Traverse the directory tree
    struct dentry *current = root_sb->root;

    for (int i = 0; i < count; i++)
    {
        if (!current || !current->inode)
            return 0;

        // Must be a directory
        if (current->inode->type != VFS_DIRECTORY)
            return 0;

        // Search for the component in children
        struct dentry *child = current->child;
        int found = 0;

        while (child)
        {
            if (strcmp(child->name, components[i]) == 0)
            {
                current = child;
                found = 1;
                break;
            }
            child = child->next;
        }

        if (!found)
            return 0;
    }

    return current;
}

// Open a file
// Helper: lookup inode by path (works with ramfs)
struct inode *vfs_lookup_inode(const char *path)
{
    if (!root_sb || !root_sb->root_inode)
        return 0;

    // Start from root
    struct inode *current = root_sb->root_inode;

    // Skip leading slash
    if (*path == '/')
        path++;

    // Skip trailing slashes
    const char *path_end = path;
    while (*path_end)
        path_end++;
    while (path_end > path && *(path_end - 1) == '/')
        path_end--;

    // If empty path, return root
    if (*path == '\0' || path == path_end)
        return current;

    // Parse path component by component
    char component[VFS_NAME_MAX];
    int comp_len;
    char current_path[256];
    int path_pos = 0;
    current_path[0] = '/';
    current_path[1] = '\0';
    path_pos = 1;

    while (path < path_end && *path)
    {
        // Extract component
        comp_len = 0;
        while (path < path_end && *path && *path != '/' && comp_len < VFS_NAME_MAX - 1)
        {
            component[comp_len++] = *path++;
        }
        component[comp_len] = '\0';

        // Skip if empty component (double slashes)
        if (comp_len == 0)
        {
            if (*path == '/')
                path++;
            continue;
        }

        // Build current path for mount check
        if (path_pos < 255)
        {
            for (int i = 0; i < comp_len && path_pos < 255; i++)
                current_path[path_pos++] = component[i];
            current_path[path_pos] = '\0';
        }

        // Skip slash
        if (*path == '/')
        {
            path++;
            if (path_pos < 255)
            {
                current_path[path_pos++] = '/';
                current_path[path_pos] = '\0';
            }
        }

        // Check if current path is a mount point
        extern struct superblock *mount_get_sb(const char *path);

        // Remove trailing slash for mount check
        char mount_check_path[256];
        int mcp = 0;
        for (int i = 0; current_path[i] && i < 255; i++)
        {
            mount_check_path[mcp++] = current_path[i];
        }
        if (mcp > 1 && mount_check_path[mcp - 1] == '/')
            mcp--;
        mount_check_path[mcp] = '\0';

        struct superblock *mounted_sb = mount_get_sb(mount_check_path);
        if (mounted_sb && mounted_sb->root_inode)
        {
            // This is a mount point! Switch to mounted filesystem
            current = mounted_sb->root_inode;

            // If there's more path, continue from mounted fs root
            if (*path == '\0')
                return current;

            // Continue with remaining path
            continue;
        }

        // Normal lookup in current directory
        if (current->i_op && current->i_op->lookup)
        {
            current = current->i_op->lookup(current, component);
            if (!current)
                return 0; // Component not found
        }
        else
        {
            return 0; // No lookup operation
        }
    }

    return current;
}

struct file *vfs_open(const char *path, uint32_t flags)
{
    (void)flags;

    // Use new inode lookup
    struct inode *inode = vfs_lookup_inode(path);
    if (!inode)
        return 0;

    struct file *file = (struct file *)kmalloc(sizeof(struct file));
    if (!file)
        return 0;

    file->inode = inode;
    file->flags = flags;
    file->pos = 0;
    file->ref_count = 1;
    file->f_op = inode->f_op;
    file->private_data = 0;

    // Call open handler if available
    if (file->f_op && file->f_op->open)
    {
        if (file->f_op->open(file->inode, file) < 0)
        {
            kfree(file);
            return 0;
        }
    }

    return file;
}

// Close a file
int vfs_close(struct file *file)
{
    if (!file)
        return -1;

    // Call close handler if available
    if (file->f_op && file->f_op->close)
    {
        file->f_op->close(file);
    }

    file->ref_count--;
    if (file->ref_count == 0)
    {
        kfree(file);
    }

    return 0;
}

// Read from a file
int vfs_read(struct file *file, char *buffer, uint32_t size)
{
    if (!file || !file->f_op || !file->f_op->read)
        return -1;

    int ret = file->f_op->read(file, buffer, size, file->pos);
    if (ret > 0)
    {
        file->pos += ret;
    }

    return ret;
}

// Write to a file
int vfs_write(struct file *file, const char *buffer, uint32_t size)
{
    if (!file || !file->f_op || !file->f_op->write)
        return -1;

    int ret = file->f_op->write(file, buffer, size, file->pos);
    if (ret > 0)
    {
        file->pos += ret;
    }

    return ret;
}

// Seek in a file
int vfs_lseek(struct file *file, uint32_t offset, int whence)
{
    if (!file)
        return -1;

    if (file->f_op && file->f_op->lseek)
    {
        return file->f_op->lseek(file, offset, whence);
    }

    // Default seek implementation
    switch (whence)
    {
    case 0: // SEEK_SET
        file->pos = offset;
        break;
    case 1: // SEEK_CUR
        file->pos += offset;
        break;
    case 2: // SEEK_END
        file->pos = file->inode->size + offset;
        break;
    default:
        return -1;
    }

    return file->pos;
}

// Create a file
struct inode *vfs_create(const char *path, uint32_t mode)
{
    (void)path;
    (void)mode;
    // TODO: Implement file creation
    return 0;
}

// Delete a file
int vfs_unlink(const char *path)
{
    if (!path || !root_sb || !root_sb->root_inode)
        return -1;

    // Parse parent directory path and filename
    char parent_path[256];
    char filename[64];

    // Find last '/'
    int last_slash = -1;
    for (int i = 0; path[i]; i++)
    {
        if (path[i] == '/')
            last_slash = i;
    }

    if (last_slash < 0)
        return -1; // Invalid path

    // Extract parent path and filename
    if (last_slash == 0)
    {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    }
    else
    {
        for (int i = 0; i < last_slash; i++)
            parent_path[i] = path[i];
        parent_path[last_slash] = '\0';
    }

    int fn_idx = 0;
    for (int i = last_slash + 1; path[i] && fn_idx < 63; i++)
        filename[fn_idx++] = path[i];
    filename[fn_idx] = '\0';

    // Look up parent directory
    struct inode *parent_inode = vfs_lookup_inode(parent_path);
    if (!parent_inode)
        return -1;

    // Call filesystem-specific unlink
    if (parent_inode->i_op && parent_inode->i_op->unlink)
    {
        return parent_inode->i_op->unlink(parent_inode, filename);
    }

    return -1;
}

// Get root superblock
struct superblock *vfs_get_root_sb(void)
{
    return root_sb;
}

// Create directory
int vfs_mkdir(const char *path, uint32_t mode)
{
    if (!path || !root_sb || !root_sb->root_inode)
        return -1;

    // Use filesystem-specific mkdir through ramfs
    extern struct inode *ramfs_create_dir(const char *path, uint32_t mode);
    struct inode *inode = ramfs_create_dir(path, mode);

    return inode ? 0 : -1;
}

// Remove directory
int vfs_rmdir(const char *path)
{
    if (!path || !root_sb || !root_sb->root_inode)
        return -1;

    // Skip leading slash
    if (*path == '/')
        path++;

    if (*path == '\0')
        return -1; // Can't remove root

    // Find parent directory and directory name
    char full_path[VFS_NAME_MAX];
    char dirname[VFS_NAME_MAX];
    int i = 0;
    int last_slash = -1;

    // Copy path and find last slash
    while (*path && i < VFS_NAME_MAX - 1)
    {
        full_path[i] = *path;
        if (*path == '/')
            last_slash = i;
        path++;
        i++;
    }
    full_path[i] = '\0';

    // Extract directory name
    int name_start = (last_slash >= 0) ? (last_slash + 1) : 0;
    int name_idx = 0;
    for (int j = name_start; j < i; j++)
    {
        dirname[name_idx++] = full_path[j];
    }
    dirname[name_idx] = '\0';

    // Get parent directory
    struct inode *parent;
    if (last_slash >= 0)
    {
        full_path[last_slash] = '\0'; // Truncate to get parent path
        parent = vfs_lookup_inode(full_path[0] ? full_path : "/");
    }
    else
    {
        parent = root_sb->root_inode; // Parent is root
    }

    if (!parent || parent->type != VFS_DIRECTORY)
        return -1;

    // Call filesystem-specific rmdir
    if (parent->i_op && parent->i_op->rmdir)
    {
        return parent->i_op->rmdir(parent, dirname);
    }

    return -1;
}

// Initialize VFS
void vfs_init(void)
{
    // Allocate root superblock
    root_sb = (struct superblock *)kmalloc(sizeof(struct superblock));
    if (!root_sb)
        return;

    root_sb->magic = 0xDEADBEEF;
    root_sb->block_size = 512;
    root_sb->max_files = 1024;
    root_sb->private_data = 0;

    // Create root inode
    root_sb->root_inode = vfs_alloc_inode(root_sb);
    if (!root_sb->root_inode)
    {
        kfree(root_sb);
        return;
    }

    root_sb->root_inode->type = VFS_DIRECTORY;
    root_sb->root_inode->mode = 0755;

    // Create root dentry
    root_sb->root = vfs_alloc_dentry("/", root_sb->root_inode);
    if (!root_sb->root)
    {
        vfs_free_inode(root_sb->root_inode);
        kfree(root_sb);
        return;
    }
}
