#include "exec.h"
#include "vfs.h"
#include "ramfs.h"
#include "kmalloc.h"
#include <stdint.h>

// Simple test program code
// This is machine code for a minimal x86 program that prints "Hello from exec!"

static unsigned char test_program_code[] = {
    // This would be actual x86 machine code
    // For now, just a placeholder that returns
    0xB8, 0x00, 0x00, 0x00, 0x00, // mov eax, 0
    0xC3                          // ret
};

// Create a test executable in memory
void create_test_programs(void)
{
    // Create a simple test program
    struct exec_header header;
    header.magic = EXEC_MAGIC;
    header.entry = USER_TEXT_START;
    header.text_size = sizeof(test_program_code);
    header.data_size = 0;
    header.bss_size = 0;
    header.stack_size = 0x4000; // 16KB stack

    // Calculate total size
    uint32_t total_size = sizeof(header) + header.text_size;

    // Allocate buffer
    char *buffer = (char *)kmalloc(total_size);
    if (!buffer)
    {
        return;
    }

    // Copy header
    char *ptr = buffer;
    for (uint32_t i = 0; i < sizeof(header); i++)
    {
        ptr[i] = ((char *)&header)[i];
    }
    ptr += sizeof(header);

    // Copy code
    for (uint32_t i = 0; i < header.text_size; i++)
    {
        ptr[i] = test_program_code[i];
    }

    // Create file in ramfs (in root directory, since ramfs doesn't support subdirectories yet)
    ramfs_create_file("/test.bin", buffer);

    kfree(buffer);
}
