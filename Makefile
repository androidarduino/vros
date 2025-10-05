# Makefile for our simple kernel

ARCH := i686
# TARGET := $(ARCH)-elf

CC := gcc
AS := as
LD := ld

CFLAGS := -Wall -Wextra -nostdlib -nostartfiles -ffreestanding -m32 -c
LDFLAGS := -T linker.ld -m elf_i386

SRCS_ASM := boot.s idt_load.s isr.s task_switch.s syscall_asm.s
SRCS_C := kernel.c idt.c isr_handler.c pic.c keyboard.c shell.c pmm.c paging.c kmalloc.c task.c syscall.c vfs.c ramfs.c procfs.c devfs.c

OBJS_ASM := $(SRCS_ASM:.s=.o)
OBJS_C := $(SRCS_C:.c=.o)

KERNEL := kernel.bin

.PHONY: all clean run image

all: $(KERNEL)

$(KERNEL): $(OBJS_ASM) $(OBJS_C)
	$(LD) $(LDFLAGS) -o $@ $^ 

%.o: %.s
	$(AS) --32 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS_ASM) $(OBJS_C) $(KERNEL) *.iso

# Run in QEMU
run: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL)

# Create a GRUB bootable ISO image
image: $(KERNEL)
	mkdir -p iso/boot/grub
	cp $(KERNEL) iso/boot/
	echo 'set timeout=0\nset default=0\n\nmenuentry "My Kernel" {\n\tmultiboot /boot/$(KERNEL)\n}' > iso/boot/grub/grub.cfg
	grub-mkrescue -o kernel.iso iso
