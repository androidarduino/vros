# Makefile for VROS Microkernel

# Compiler and tools
ARCH := i686
CC := gcc
AS := as
LD := ld

# Directories
SRC_DIR := src
BOOT_DIR := $(SRC_DIR)/boot
KERNEL_DIR := $(SRC_DIR)/kernel
MM_DIR := $(SRC_DIR)/mm
FS_DIR := $(SRC_DIR)/fs
DRIVERS_DIR := $(SRC_DIR)/drivers
LIB_DIR := $(SRC_DIR)/lib
INCLUDE_DIR := $(SRC_DIR)/include

BUILD_DIR := build
ISO_DIR := iso

# Compiler flags
CFLAGS := -Wall -Wextra -nostdlib -nostartfiles -ffreestanding -m32 -c -I$(INCLUDE_DIR)
ASFLAGS := --32
LDFLAGS := -T linker.ld -m elf_i386

# Source files
SRCS_ASM := $(BOOT_DIR)/boot.s \
            $(KERNEL_DIR)/idt_load.s \
            $(KERNEL_DIR)/isr.s \
            $(KERNEL_DIR)/task_switch.s \
            $(KERNEL_DIR)/syscall_asm.s

SRCS_C := $(KERNEL_DIR)/kernel.c \
          $(KERNEL_DIR)/idt.c \
          $(KERNEL_DIR)/isr_handler.c \
          $(KERNEL_DIR)/pic.c \
          $(KERNEL_DIR)/syscall.c \
          $(KERNEL_DIR)/task.c \
          $(KERNEL_DIR)/ipc.c \
          $(KERNEL_DIR)/ioport.c \
          $(KERNEL_DIR)/irq_bridge.c \
          $(KERNEL_DIR)/driver_config.c \
          $(MM_DIR)/pmm.c \
          $(MM_DIR)/paging.c \
          $(MM_DIR)/kmalloc.c \
          $(FS_DIR)/vfs.c \
          $(FS_DIR)/ramfs.c \
          $(FS_DIR)/procfs.c \
          $(FS_DIR)/devfs.c \
          $(FS_DIR)/exec.c \
          $(FS_DIR)/vrfs.c \
          $(FS_DIR)/mount.c \
          $(DRIVERS_DIR)/keyboard.c \
          $(DRIVERS_DIR)/ata.c \
          $(DRIVERS_DIR)/blkdev.c \
          $(DRIVERS_DIR)/ata_blk.c \
          $(DRIVERS_DIR)/ne2000.c \
          $(DRIVERS_DIR)/netif.c \
          $(DRIVERS_DIR)/blkdev_ipc_client.c \
          $(DRIVERS_DIR)/netdev_ipc_client.c \
          $(LIB_DIR)/netstack_driver.c \
          $(LIB_DIR)/shell.c \
          $(LIB_DIR)/usermode.c \
          $(LIB_DIR)/user_prog.c \
          $(LIB_DIR)/test_prog.c \
          $(LIB_DIR)/sched_test.c \
          $(LIB_DIR)/ipc_test.c \
          $(LIB_DIR)/userspace_driver.c \
          $(LIB_DIR)/ioport_test.c \
          $(LIB_DIR)/ata_driver.c \
          $(LIB_DIR)/ne2000_driver.c

# Object files (in build directory)
OBJS_ASM := $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.o,$(SRCS_ASM))
OBJS_C := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS_C))
OBJS := $(OBJS_ASM) $(OBJS_C)

# Output
KERNEL := kernel.bin

.PHONY: all clean run image tree

all: $(KERNEL)

# Link kernel
$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "✓ Kernel built successfully: $(KERNEL)"

# Compile assembly files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(KERNEL) $(ISO_DIR) *.iso
	@echo "✓ Cleaned build artifacts"

# Create disk image if it doesn't exist
disk.img:
	qemu-img create -f raw disk.img 16M

# Run in QEMU with disk and network (specify raw format to allow block 0 writes)
# Using legacy -net syntax with ICMP support
run: $(KERNEL) disk.img
	qemu-system-x86_64 -kernel $(KERNEL) -drive file=disk.img,format=raw,index=0,media=disk -net nic,model=ne2k_isa -net user,restrict=no

# Run with TAP network (real network interface, requires sudo ./setup-tap.sh first)
run-tap: $(KERNEL) disk.img
	@echo "================================================"
	@echo "Starting QEMU with TAP network..."
	@echo "Make sure you ran: sudo ./setup-tap.sh"
	@echo "================================================"
	sudo qemu-system-x86_64 -kernel $(KERNEL) -drive file=disk.img,format=raw,index=0,media=disk -net nic,model=ne2k_isa -net tap,ifname=tap0,script=no,downscript=no

# Create a GRUB bootable ISO image
image: $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/
	echo 'set timeout=0\nset default=0\n\nmenuentry "VROS" {\n\tmultiboot /boot/$(KERNEL)\n}' > $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o vros.iso $(ISO_DIR)
	@echo "✓ Bootable ISO created: vros.iso"

# Show project structure
tree:
	@echo "VROS Project Structure:"
	@tree -L 3 --charset ascii -I 'build|*.o|*.bin|*.iso'

# Help
help:
	@echo "VROS Makefile Commands:"
	@echo "  make           - Build the kernel"
	@echo "  make run       - Build and run in QEMU"
	@echo "  make clean     - Clean build artifacts"
	@echo "  make image     - Create bootable ISO"
	@echo "  make tree      - Show project structure"
	@echo "  make help      - Show this help"
