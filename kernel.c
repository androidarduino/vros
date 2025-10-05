// kernel.c

#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "keyboard.h"
#include "shell.h"
#include "multiboot.h"
#include "pmm.h"
#include "paging.h"
#include "kmalloc.h"
#include "task.h"
#include "syscall.h"
#include "vfs.h"
#include "ramfs.h"
#include "procfs.h"
#include "devfs.h"

// VGA text mode buffer address
volatile char *vga_buffer = (volatile char *)0xB8000;

// Function to print a single character to the VGA buffer
void print_char(char c, int col, int row)
{
    int index = (row * 80 + col) * 2;
    vga_buffer[index] = c;
    vga_buffer[index + 1] = 0x07; // Light grey on black
}

// Function to print a string to the VGA buffer
void print_string(const char *str, int row)
{
    int col = 0;
    while (*str != '\0')
    {
        print_char(*str, col++, row);
        str++;
    }
}

// External multiboot info pointer
extern uint32_t multiboot_info_ptr;

void kernel_main(void)
{
    // Print a welcome message
    print_string("Hello from my microkernel!", 0);

    // Get multiboot info
    struct multiboot_info *mbi = (struct multiboot_info *)multiboot_info_ptr;

    print_string("Initializing IDT...", 1);

    // Initialize IDT
    idt_init();

    print_string("IDT initialized successfully!", 2);
    print_string("Installing ISRs...", 3);

    // Install ISRs
    isr_install();

    print_string("ISRs installed successfully!", 4);

    // Initialize memory management
    print_string("Initializing memory manager...", 5);

    // Calculate total memory (in KB from multiboot)
    uint32_t mem_size = (mbi->mem_lower + mbi->mem_upper) * 1024;
    pmm_init(mem_size);

    // Mark available memory regions (simple: assume 1MB-16MB is available)
    pmm_init_region(0x100000, 15 * 1024 * 1024); // 15MB starting at 1MB

    // Reserve kernel memory (first 1MB)
    pmm_deinit_region(0, 0x100000);

    print_string("Memory manager initialized!", 6);

    // Initialize paging
    print_string("Initializing paging...", 7);
    paging_init();
    paging_enable();
    print_string("Paging enabled!", 8);

    // Initialize kernel heap (1MB heap starting at 4MB)
    print_string("Initializing kernel heap...", 9);
    kmalloc_init((void *)0x00400000, 1024 * 1024); // 1MB heap at 4MB
    print_string("Heap initialized!", 10);

    print_string("Initializing PIC and IRQs...", 11);

    // Initialize PIC
    pic_init();

    // Install IRQ handlers
    irq_install();

    print_string("Interrupts ready! Enabling...", 12);

    // Enable interrupts
    __asm__ volatile("sti");

    print_string("Interrupts enabled!", 13);
    print_string("Initializing keyboard...", 14);

    // Initialize keyboard
    keyboard_init();

    print_string("Keyboard ready!", 15);

    // Initialize multitasking
    print_string("Initializing multitasking...", 16);
    task_init();
    print_string("Multitasking enabled!", 17);

    // Initialize system calls
    print_string("Initializing system calls...", 18);
    syscall_init();
    print_string("System calls enabled!", 19);

    // Initialize VFS
    print_string("Initializing VFS...", 20);
    vfs_init();
    ramfs_init();
    ramfs_mount_root();
    print_string("VFS initialized!", 21);

    // Create some test files
    ramfs_create_file("/hello.txt", "Hello, World!\n");
    ramfs_create_file("/test.txt", "This is a test file.\n");

    // Mount procfs
    print_string("Mounting procfs...", 22);
    procfs_init();
    procfs_mount();
    print_string("procfs mounted at /proc!", 23);

    // Mount devfs
    print_string("Mounting devfs...", 24);
    devfs_init();
    devfs_mount();
    print_string("devfs mounted at /dev!", 25);

    print_string("Starting shell...", 26);

    // Wait a moment
    for (volatile int i = 0; i < 10000000; i++)
        ;

    // Initialize and start shell
    shell_init();
    keyboard_enable_shell();

    // Loop indefinitely
    while (1)
    {
        // Halt CPU until next interrupt
        __asm__ volatile("hlt");
    }
}
