.section .text
.global syscall_asm_handler
.extern syscall_handler

syscall_asm_handler:
    cli
    
    # Save all registers
    pusha
    
    # Save data segment
    mov %ds, %ax
    push %eax
    
    # Load kernel data segment
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    # Push ESP (pointer to registers struct)
    push %esp
    
    # Call C handler
    call syscall_handler
    
    # Clean up stack
    add $4, %esp
    
    # Restore data segment
    pop %ebx
    mov %bx, %ds
    mov %bx, %es
    mov %bx, %fs
    mov %bx, %gs
    
    # Restore all registers (except EAX which contains return value)
    popa
    
    sti
    iret

