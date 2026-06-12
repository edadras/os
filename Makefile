OS_NAME := ariaos

CC      := gcc
AS      := nasm
LD      := ld

CFLAGS  := -m32 -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
           -fno-builtin -nostdlib -O2 -Wall -Wextra -Ikernel
ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

BUILD   := build
ISO_DIR := $(BUILD)/iso

C_SRCS  := $(wildcard kernel/*.c)
A_SRCS  := boot/boot.asm kernel/interrupts.asm
OBJS    := $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS)) \
           $(patsubst %.asm,$(BUILD)/%.o,$(A_SRCS))

KERNEL  := $(BUILD)/kernel.elf
ISO     := $(OS_NAME).iso

.PHONY: all iso run clean

all: iso

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(KERNEL) grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null

run: iso
	qemu-system-i386 -cdrom $(ISO) -m 128

clean:
	rm -rf $(BUILD) $(ISO)
