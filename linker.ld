/*
 * linker.ld
 * Minimal linker script for a simple x86 kernel.
 */

ENTRY(_start)

SECTIONS {
    . = 0x100000; /* Kernel starts at 1MB */

    .text : {
        *(.multiboot)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.note.GNU-stack)
    }
}
