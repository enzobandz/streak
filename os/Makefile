# StreakOS build. Needs: gcc (with -m32 support), nasm, ld (GNU binutils),
# grub-mkrescue + xorriso (+ grub-pc-bin / grub-efi-amd64-bin for a hybrid ISO).
CC      ?= gcc
LD      ?= ld
NASM    ?= nasm
CFLAGS  := -m32 -march=i686 -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
           -fno-builtin -fno-asynchronous-unwind-tables -fno-math-errno -mno-sse -mno-mmx \
           -nostdlib -Wall -Wextra -Wno-unused-parameter -std=gnu11 -Ikernel
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

BUILD := build
CSRC  := $(wildcard kernel/*.c) $(wildcard apps/*.c)
ASRC  := boot/boot.asm boot/isr.asm
OBJS  := $(ASRC:%.asm=$(BUILD)/%.o) $(CSRC:%.c=$(BUILD)/%.o)
KERNEL := $(BUILD)/streakos.elf
ISO    := $(BUILD)/streakos.iso

.PHONY: all iso run run-kvm clean
all: iso

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf32 $< -o $@

$(BUILD)/%.o: %.c kernel/kernel.h kernel/wm.h kernel/font.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

iso: $(ISO)
$(ISO): $(KERNEL) grub.cfg
	@mkdir -p $(BUILD)/iso/boot/grub
	cp $(KERNEL) $(BUILD)/iso/boot/streakos.elf
	cp grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILD)/iso 2>/dev/null

run: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -m 256M -serial stdio -vga std

run-kvm: $(ISO)
	qemu-system-x86_64 -enable-kvm -cdrom $(ISO) -m 256M -serial stdio -vga std

clean:
	rm -rf $(BUILD)
