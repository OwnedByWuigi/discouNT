BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
DISCOUNT_DIR := $(ISO_DIR)/DISCOUNT
SYSTEM32_DIR := $(DISCOUNT_DIR)/SYSTEM32
GRUB_DIR := $(ISO_DIR)/boot/grub
KERNEL_ISO_PATH := $(SYSTEM32_DIR)/NTOSKRNL.EXE
WEB_DIR := $(DISCOUNT_DIR)/Web
WALLPAPER_FILES := $(wildcard media/wallpaper/*)
WALLPAPER_STAMP := $(WEB_DIR)/.stamp

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
BOOT_OBJ := $(BUILD_DIR)/boot/boot.o

CC := clang
LD := ld
NASM := nasm
GRUB_MKRESCUE := grub-mkrescue
MINGW_CC := i686-w64-mingw32-gcc
QEMU_AUDIODEV ?= driver=alsa,id=audio0
# QEMU's ES1370 model is register-compatible with the ES1371 path used by
# VMware. Override this when testing the alternate endpoint.
QEMU_SOUND_DEVICE ?= AC97

CPPFLAGS := \
	-Iinclude/win32 \
	-Ikernel \
	-Iwin32 \
	-Idrivers/cdfs \
	-Idrivers/fb \
	-Idrivers/keyboard \
	-Idrivers/mouse \
	-Idrivers/net \
	-Idrivers/usb \
	-Idrivers/ide \
	-Idrivers/ahci \
	-Idrivers/audio \
	-Idrivers/fat32 \
	-Idrivers/input \
	-Idrivers/serial \
	-Idrivers/vga \
	-Iwin32/w32k \
	-Iwin32/smss \
	-Iwin32/csrss

WDF_PLATFORM_CPPFLAGS := -Icompat/wdf/platform/include
UMDFHOST_APP := $(BUILD_DIR)/apps/umdfhost.exe

CFLAGS := \
	-ffreestanding \
	-nostdlib \
	-fno-builtin \
	-fno-stack-protector \
	-nostartfiles \
	-m32 \
	-fno-pic \
	-O0

LDFLAGS := -m elf_i386 -T kernel/arch/x86/linker.ld -nostdlib -no-pie

