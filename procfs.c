#include "procfs.h"
#include "kmalloc.h"
#include "pmm.h"
#include "task.h"

// External timer ticks
extern volatile uint32_t timer_ticks;

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

// Helper: int to string
static void int_to_str(int num, char *str)
{
    if (num == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    int is_negative = 0;
    if (num < 0)
    {
        is_negative = 1;
        num = -num;
    }

    char temp[20];
    int i = 0;

    while (num > 0)
    {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }

    int j = 0;
    if (is_negative)
    {
        str[j++] = '-';
    }

    while (i > 0)
    {
        str[j++] = temp[--i];
    }

    str[j] = '\0';
}

// Helper: uint32 to string
static void uint32_to_str(uint32_t num, char *str)
{
    if (num == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    char temp[20];
    int i = 0;

    while (num > 0)
    {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }

    int j = 0;
    while (i > 0)
    {
        str[j++] = temp[--i];
    }

    str[j] = '\0';
}

// Helper: append string
static void strcat(char *dest, const char *src)
{
    while (*dest)
        dest++;
    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Generate uptime content
static char *procfs_generate_uptime(void)
{
    char *buffer = (char *)kmalloc(256);
    if (!buffer)
        return 0;

    uint32_t seconds = timer_ticks / 18; // ~18.2 ticks per second

    strcpy(buffer, "Uptime: ");
    char num_str[20];
    uint32_to_str(seconds, num_str);
    strcat(buffer, num_str);
    strcat(buffer, " seconds\n");

    return buffer;
}

// Generate meminfo content
static char *procfs_generate_meminfo(void)
{
    char *buffer = (char *)kmalloc(512);
    if (!buffer)
        return 0;

    // Initialize buffer
    buffer[0] = '\0';

    strcpy(buffer, "Memory Information:\n");

    // Get memory stats with safety checks
    uint32_t total_blocks = pmm_get_total_blocks();
    uint32_t used_blocks = pmm_get_used_blocks();

    // Safety check
    if (total_blocks == 0 || total_blocks > 1000000)
    {
        strcat(buffer, "  Error: Invalid memory data\n");
        return buffer;
    }

    if (used_blocks > total_blocks)
    {
        used_blocks = total_blocks;
    }

    uint32_t free_blocks = total_blocks - used_blocks;

    // Total memory (avoid overflow by checking before multiplication)
    strcat(buffer, "  Total blocks:  ");
    char num_str[32];
    uint32_to_str(total_blocks, num_str);
    strcat(buffer, num_str);
    strcat(buffer, "\n");

    // Used memory
    strcat(buffer, "  Used blocks:   ");
    uint32_to_str(used_blocks, num_str);
    strcat(buffer, num_str);
    strcat(buffer, "\n");

    // Free memory
    strcat(buffer, "  Free blocks:   ");
    uint32_to_str(free_blocks, num_str);
    strcat(buffer, num_str);
    strcat(buffer, "\n");

    // Note about block size
    strcat(buffer, "  (1 block = 4KB)\n");

    return buffer;
}

// Generate tasks content
static char *procfs_generate_tasks(void)
{
    char *buffer = (char *)kmalloc(512);
    if (!buffer)
        return 0;

    strcpy(buffer, "Current Task:\n");

    struct task *current = task_get_current();
    if (current)
    {
        strcat(buffer, "  PID:   ");
        char num_str[20];
        int_to_str(current->pid, num_str);
        strcat(buffer, num_str);
        strcat(buffer, "\n");

        strcat(buffer, "  Name:  ");
        strcat(buffer, current->name);
        strcat(buffer, "\n");

        strcat(buffer, "  State: ");
        switch (current->state)
        {
        case TASK_RUNNING:
            strcat(buffer, "Running\n");
            break;
        case TASK_READY:
            strcat(buffer, "Ready\n");
            break;
        case TASK_BLOCKED:
            strcat(buffer, "Blocked\n");
            break;
        case TASK_ZOMBIE:
            strcat(buffer, "Zombie\n");
            break;
        default:
            strcat(buffer, "Unknown\n");
            break;
        }
    }
    else
    {
        strcat(buffer, "  No task running\n");
    }

    return buffer;
}

// procfs file operations
static int procfs_open(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

static int procfs_close(struct file *file)
{
    (void)file;
    return 0;
}

static int procfs_read(struct file *file, char *buffer, uint32_t size, uint32_t offset)
{
    struct procfs_node *node = (struct procfs_node *)file->inode->private_data;
    if (!node)
        return 0;

    // Generate content on demand
    char *content = 0;
    int content_len = 0;

    switch (node->type)
    {
    case PROCFS_UPTIME:
        content = procfs_generate_uptime();
        break;
    case PROCFS_MEMINFO:
        content = procfs_generate_meminfo();
        break;
    case PROCFS_TASKS:
        content = procfs_generate_tasks();
        break;
    default:
        return 0;
    }

    if (!content)
        return 0;

    content_len = strlen(content);

    // Check bounds
    if (offset >= (uint32_t)content_len)
    {
        kfree(content);
        return 0;
    }

    // Calculate actual read size
    uint32_t to_read = size;
    if (offset + to_read > (uint32_t)content_len)
    {
        to_read = content_len - offset;
    }

    // Copy data
    for (uint32_t i = 0; i < to_read; i++)
    {
        buffer[i] = content[offset + i];
    }

    kfree(content);
    return to_read;
}

static int procfs_write(struct file *file, const char *buffer, uint32_t size, uint32_t offset)
{
    (void)file;
    (void)buffer;
    (void)size;
    (void)offset;
    return -1; // procfs files are read-only
}

// procfs file operations table
static struct file_operations procfs_fops = {
    .open = procfs_open,
    .close = procfs_close,
    .read = procfs_read,
    .write = procfs_write,
    .lseek = 0,
    .readdir = 0};

// Create a procfs file
static struct inode *procfs_create_file(const char *name, procfs_file_type_t type)
{
    struct superblock *root_sb = vfs_get_root_sb();
    if (!root_sb)
        return 0;

    // First, find /proc directory
    struct dentry *proc_dir = vfs_lookup("/proc");
    if (!proc_dir)
        return 0;

    // Allocate new inode
    struct inode *inode = vfs_alloc_inode(root_sb);
    if (!inode)
        return 0;

    inode->type = VFS_FILE;
    inode->mode = 0444; // Read-only
    inode->f_op = &procfs_fops;

    // Allocate procfs node
    struct procfs_node *node = (struct procfs_node *)kmalloc(sizeof(struct procfs_node));
    if (!node)
    {
        vfs_free_inode(inode);
        return 0;
    }

    node->type = type;
    node->cached_data = 0;
    node->cached_size = 0;
    inode->private_data = node;

    // Create dentry
    struct dentry *dentry = vfs_alloc_dentry(name, inode);
    if (!dentry)
    {
        kfree(node);
        vfs_free_inode(inode);
        return 0;
    }

    // Add to /proc's children
    dentry->parent = proc_dir;
    dentry->next = proc_dir->child;
    proc_dir->child = dentry;

    return inode;
}

// Initialize procfs
void procfs_init(void)
{
    // Nothing to initialize here
}

// Mount procfs at /proc
int procfs_mount(void)
{
    struct superblock *root_sb = vfs_get_root_sb();
    if (!root_sb || !root_sb->root)
        return -1;

    // Create /proc directory
    struct inode *proc_inode = vfs_alloc_inode(root_sb);
    if (!proc_inode)
        return -1;

    proc_inode->type = VFS_DIRECTORY;
    proc_inode->mode = 0555; // Read and execute

    struct dentry *proc_dentry = vfs_alloc_dentry("proc", proc_inode);
    if (!proc_dentry)
    {
        vfs_free_inode(proc_inode);
        return -1;
    }

    // Add to root's children
    proc_dentry->parent = root_sb->root;
    proc_dentry->next = root_sb->root->child;
    root_sb->root->child = proc_dentry;

    // Create procfs files
    procfs_create_file("uptime", PROCFS_UPTIME);
    procfs_create_file("meminfo", PROCFS_MEMINFO);
    procfs_create_file("tasks", PROCFS_TASKS);

    return 0;
}
