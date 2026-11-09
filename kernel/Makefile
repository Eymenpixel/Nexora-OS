BUILD := build
ISO_ROOT := $(BUILD)/iso
AUTH_IMAGE := blackbit-auth.img
CC := gcc
CXX := g++
LD := ld

COMMON := -m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin -mno-sse -mno-mmx -mno-80387
CXXFLAGS := $(COMMON) -std=c++17 -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-asynchronous-unwind-tables -fno-unwind-tables -MMD -MP -O2 -Ikernel

.PHONY: all iso check run run-serial clean
all: iso

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot/boot.S | $(BUILD)
	$(CC) -m32 -c $< -o $@

$(BUILD)/interrupts.o: boot/interrupts.S | $(BUILD)
	$(CC) -m32 -c $< -o $@

$(BUILD)/user_demo.o: boot/user_demo.S | $(BUILD)
	$(CC) -m32 -c $< -o $@

$(BUILD)/%.o: kernel/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

KERNEL_OBJECTS := $(BUILD)/kernel.o $(BUILD)/arch.o $(BUILD)/memory.o $(BUILD)/paging.o $(BUILD)/getallow_policy.o $(BUILD)/auth.o $(BUILD)/auth_crypto.o $(BUILD)/auth_store.o

-include $(KERNEL_OBJECTS:.o=.d)

$(BUILD)/blackbit.elf: $(BUILD)/boot.o $(BUILD)/interrupts.o $(BUILD)/user_demo.o $(KERNEL_OBJECTS) boot/linker.ld
	$(LD) -m elf_i386 -T boot/linker.ld -nostdlib -o $@ $(BUILD)/boot.o $(BUILD)/interrupts.o $(BUILD)/user_demo.o $(KERNEL_OBJECTS)

iso: $(BUILD)/blackbit.elf
	mkdir -p $(ISO_ROOT)/boot/grub
	cp $< $(ISO_ROOT)/boot/blackbit.elf
	cp iso/boot/grub/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/blackbit.iso $(ISO_ROOT)

check: $(BUILD)/blackbit.elf
	grub-file --is-x86-multiboot2 $<

$(AUTH_IMAGE):
	umask 077; dd if=/dev/zero of=$@ bs=512 count=64 status=none
	chmod 600 $@

run: iso $(AUTH_IMAGE)
	qemu-system-i386 -cpu max -cdrom $(BUILD)/blackbit.iso -drive file=$(AUTH_IMAGE),format=raw,if=ide,index=0 -m 128M

run-serial: iso $(AUTH_IMAGE)
	qemu-system-i386 -cpu max -cdrom $(BUILD)/blackbit.iso -drive file=$(AUTH_IMAGE),format=raw,if=ide,index=0 -m 128M -display none -serial stdio -monitor none

clean:
	rm -rf $(BUILD)
