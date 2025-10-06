#include "ioport.h"
#include "task.h"
#include "kmalloc.h"
#include <stdint.h>

// I/O Permission Bitmap 大小（65536 端口 / 8 位 = 8192 字节）
#define IOPB_SIZE 8192

// TSS 结构（简化版，只包含我们需要的部分）
struct tss_entry
{
    uint32_t prev_tss;
    uint32_t esp0; // 内核栈指针
    uint32_t ss0;  // 内核栈段
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base; // I/O Permission Bitmap 偏移
} __attribute__((packed));

// 全局 TSS
static struct tss_entry tss;
static uint8_t iopb[IOPB_SIZE];

// 初始化 I/O 权限系统
void ioport_init(void)
{
    // 初始化 TSS
    for (int i = 0; i < (int)sizeof(struct tss_entry); i++)
    {
        ((uint8_t *)&tss)[i] = 0;
    }

    // 设置内核栈段
    tss.ss0 = 0x10; // 内核数据段
    tss.esp0 = 0;   // 将在任务切换时设置

    // 设置 IOPB 偏移
    tss.iomap_base = sizeof(struct tss_entry);

    // 初始化 IOPB - 默认所有端口都禁止访问（位设为1）
    for (int i = 0; i < IOPB_SIZE; i++)
    {
        iopb[i] = 0xFF;
    }

    // 在 GDT 中安装 TSS
    // 注意：这需要修改 GDT，我们稍后会添加这部分代码
}

// 为当前任务授予 I/O 端口访问权限
int ioport_grant_access(uint16_t port_start, uint16_t port_end)
{
    if (port_start > port_end || port_end >= 65536)
        return -1;

    // 获取当前任务
    struct task *current = get_current_task();
    if (!current)
        return -1;

    // 如果任务还没有 IOPB，分配一个
    if (!current->iopb)
    {
        current->iopb = (uint8_t *)kmalloc(IOPB_SIZE);
        if (!current->iopb)
            return -1;

        // 初始化为全部禁止
        for (int i = 0; i < IOPB_SIZE; i++)
        {
            current->iopb[i] = 0xFF;
        }
    }

    // 设置端口权限（清除相应的位表示允许访问）
    for (uint32_t port = port_start; port <= port_end; port++)
    {
        uint32_t byte_index = port / 8;
        uint32_t bit_index = port % 8;
        current->iopb[byte_index] &= ~(1 << bit_index);
    }

    return 0;
}

// 撤销当前任务的 I/O 端口访问权限
int ioport_revoke_access(uint16_t port_start, uint16_t port_end)
{
    if (port_start > port_end || port_end >= 65536)
        return -1;

    struct task *current = get_current_task();
    if (!current || !current->iopb)
        return -1;

    // 清除端口权限（设置相应的位表示禁止访问）
    for (uint32_t port = port_start; port <= port_end; port++)
    {
        uint32_t byte_index = port / 8;
        uint32_t bit_index = port % 8;
        current->iopb[byte_index] |= (1 << bit_index);
    }

    return 0;
}

// 检查当前任务是否有权限访问指定端口
int ioport_check_access(uint16_t port)
{
    struct task *current = get_current_task();
    if (!current || !current->iopb)
        return 0; // 没有 IOPB = 没有权限

    uint32_t byte_index = port / 8;
    uint32_t bit_index = port % 8;

    // 位为 0 表示允许访问
    return (current->iopb[byte_index] & (1 << bit_index)) == 0;
}

// 加载当前任务的 IOPB 到 TSS
void ioport_load_current_task_iopb(void)
{
    struct task *current = get_current_task();
    if (current && current->iopb)
    {
        // 复制任务的 IOPB 到全局 IOPB
        for (int i = 0; i < IOPB_SIZE; i++)
        {
            iopb[i] = current->iopb[i];
        }
    }
    else
    {
        // 没有 IOPB，禁止所有端口访问
        for (int i = 0; i < IOPB_SIZE; i++)
        {
            iopb[i] = 0xFF;
        }
    }
}
