# Enixnel kernel build

CC ?= i686-elf-gcc
LD ?= i686-elf-ld
AS = $(CC)

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -m32
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib -z max-page-size=0x1000

SRCDIR = .
BUILDDIR = build
INCLUDEDIR = include

ARCH_SRCS = \
    arch/x86/boot/multiboot_header.S \
    arch/x86/boot/boot.S

KERNEL_SRCS = \
    kernel/main.c \
    kernel/crtfiles.c \
    kernel/delfiles.c

OBJS = $(ARCH_SRCS:%.S=$(BUILDDIR)/%.o) \
       $(KERNEL_SRCS:%.c=$(BUILDDIR)/%.o)

TARGET = $(BUILDDIR)/kernel.elf
ISO = enixnel.iso

.PHONY: all clean run iso dirs

all: $(TARGET)

dirs:
	mkdir -p $(BUILDDIR)/arch/x86/boot
	mkdir -p $(BUILDDIR)/kernel

$(BUILDDIR)/%.o: %.S | dirs
	$(AS) $(CFLAGS) -I$(INCLUDEDIR) -c $< -o $@

$(BUILDDIR)/%.o: %.c | dirs
	$(CC) $(CFLAGS) -I$(INCLUDEDIR) -c $< -o $@

$(TARGET): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: all
	mkdir -p iso/boot/grub
	cp $(TARGET) iso/boot/enixnel.elf
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "Enixnel" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot /boot/enixnel.elf' >> iso/boot/grub/grub.cfg
	echo '  boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

run: iso
	qemu-system-i386 -cdrom $(ISO)

clean:
	rm -rf $(BUILDDIR) iso $(ISO)