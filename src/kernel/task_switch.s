.global task_switch

# void task_switch(struct registers_state* old_regs, struct registers_state* new_regs)
task_switch:
    # Get parameters from stack
    mov 4(%esp), %eax  # old_regs
    mov 8(%esp), %edx  # new_regs
    
    # Save current task's registers to old_regs
    mov %ebx, 4(%eax)   # ebx
    mov %ecx, 8(%eax)   # ecx
    mov %esi, 16(%eax)  # esi
    mov %edi, 20(%eax)  # edi
    mov %ebp, 24(%eax)  # ebp
    
    # Save current ESP (before call)
    mov %esp, %ebx
    add $4, %ebx        # Adjust for return address
    mov %ebx, 28(%eax)  # esp
    
    # Save EIP (return address)
    mov (%esp), %ebx
    mov %ebx, 32(%eax)  # eip
    
    # Save EFLAGS
    pushf
    pop %ebx
    mov %ebx, 36(%eax)  # eflags
    
    # Save CR3 (page directory)
    mov %cr3, %ebx
    mov %ebx, 40(%eax)  # cr3
    
    # Load new task's CR3
    mov 40(%edx), %ebx
    mov %ebx, %cr3
    
    # Load new task's registers
    mov 4(%edx), %ebx   # ebx
    mov 8(%edx), %ecx   # ecx
    mov 16(%edx), %esi  # esi
    mov 20(%edx), %edi  # edi
    mov 24(%edx), %ebp  # ebp
    mov 28(%edx), %esp  # esp
    
    # Load EFLAGS
    mov 36(%edx), %eax
    push %eax
    popf
    
    # Jump to new task's EIP
    mov 32(%edx), %eax
    jmp *%eax

