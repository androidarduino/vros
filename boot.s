.section .multiboot
.align 4
multiboot_header:
    .long 0x1BADB002             
    .long 0x00000000             
    .long -(0x1BADB002 + 0x00000000) 

.section .text
.global _start
.extern kernel_main

_start:
    cli

    # Save multiboot info pointer (passed in EBX)
    mov %ebx, multiboot_info_ptr

    # Set up a stack
    mov $stack_top, %esp

    # Set up a minimal GDT
    lgdt gdt_descriptor

    # Reload segment registers
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    call kernel_main                
    hlt                             

.section .data
# Global Descriptor Table
gdt_start:
    .quad 0        # Null descriptor

# Code Segment Descriptor
gdt_code:
    .word 0xFFFF    # Limit (low)
    .word 0x0000    # Base (low)
    .byte 0x00      # Base (middle)
    .byte 0x9A      # Access Byte (Present, Ring 0, Code, Readable)
    .byte 0xCF      # High flags (Granularity 4KB, 32-bit, Limit high)
    .byte 0x00      # Base (high)

# Data Segment Descriptor
gdt_data:
    .word 0xFFFF    # Limit (low)
    .word 0x0000    # Base (low)
    .byte 0x00      # Base (middle)
    .byte 0x92      # Access Byte (Present, Ring 0, Data, Writable)
    .byte 0xCF      # High flags (Granularity 4KB, 32-bit, Limit high)
    .byte 0x00      # Base (high)

# User Code Segment Descriptor (Ring 3)
gdt_user_code:
    .word 0xFFFF    # Limit (low)
    .word 0x0000    # Base (low)
    .byte 0x00      # Base (middle)
    .byte 0xFA      # Access Byte (Present, Ring 3, Code, Readable)
    .byte 0xCF      # High flags (Granularity 4KB, 32-bit, Limit high)
    .byte 0x00      # Base (high)

# User Data Segment Descriptor (Ring 3)
gdt_user_data:
    .word 0xFFFF    # Limit (low)
    .word 0x0000    # Base (low)
    .byte 0x00      # Base (middle)
    .byte 0xF2      # Access Byte (Present, Ring 3, Data, Writable)
    .byte 0xCF      # High flags (Granularity 4KB, 32-bit, Limit high)
    .byte 0x00      # Base (high)

gdt_end:

gdt_descriptor:
    .word gdt_end - gdt_start - 1 # GDT Limit
    .long gdt_start             # GDT Base

.section .data
.global multiboot_info_ptr
multiboot_info_ptr:
    .long 0

.section .bss
.align 16
stack_bottom:
    .skip 4096 
stack_top:
