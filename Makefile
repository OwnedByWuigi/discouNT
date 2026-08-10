BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
SYSTEM32_DIR := $(ISO_DIR)/SYSTEM32
GRUB_DIR := $(ISO_DIR)/boot/grub

ISO_NAME := ntos.iso
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
BOOT_OBJ := $(BUILD_DIR)/boot/boot.o

CC := gcc
LD := ld
NASM := nasm
GRUB_MKRESCUE := grub-mkrescue
MINGW_CC := i686-w64-mingw32-gcc

CPPFLAGS := \
	-Ikernel \
	-Iwin32 \
	-Idrivers/cdfs \
	-Idrivers/fb \
	-Idrivers/keyboard \
	-Idrivers/mouse \
	-Idrivers/serial \
	-Idrivers/vga \
	-Idrivers/win32k \
	-Iwin32/smss

CFLAGS := \
	-ffreestanding \
	-nostdlib \
	-fno-builtin \
	-fno-stack-protector \
	-nostartfiles \
	-m32 \
	-fno-pic \
	-O0

LDFLAGS := -m elf_i386 -T kernel/linker.ld -nostdlib -no-pie

KERNEL_CORE_SRCS := \
	kernel/util.c \
	kernel/mm.c \
	kernel/hal.c \
	kernel/object.c \
	kernel/ke.c \
	kernel/peloader.c \
	kernel/subsystem.c \
	kernel/nativecmd.c \
	win32/smss/smss.c \
	kernel/entry.c

DRIVER_SRCS := \
	drivers/serial/serial.c \
	drivers/cdfs/cdfs.c \
	drivers/vga/vga.c \
	drivers/fb/fb.c \
	drivers/keyboard/keyboard.c \
	drivers/win32k/win32k.c \
	drivers/mouse/mouse.c

KERNEL_SRCS := $(KERNEL_CORE_SRCS) $(DRIVER_SRCS)
KERNEL_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))

DLL_SRCS := $(wildcard dlls/*/*.c)
DLL_NAMES := $(sort $(notdir $(basename $(DLL_SRCS))))
DLL_OUTPUTS := $(addprefix $(BUILD_DIR)/dlls/,$(addsuffix .dll,$(DLL_NAMES)))

APP_SRC_FILES := $(wildcard apps/*.c) $(wildcard apps/*/*.c)
BUILT_APP_FILES := $(patsubst apps/%.c,$(BUILD_DIR)/apps/%.exe,$(APP_SRC_FILES))
SMSS_APP := $(BUILD_DIR)/win32/smss/smss.exe

.PHONY: all clean iso kernel dlls apps run

all: $(ISO_NAME)

kernel: $(KERNEL_ELF)

dlls: $(DLL_OUTPUTS)

apps: $(BUILT_APP_FILES) $(SMSS_APP)

$(ISO_NAME): $(KERNEL_ELF) $(DLL_OUTPUTS) $(GRUB_DIR)/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(ISO_DIR)

$(KERNEL_ELF): $(BOOT_OBJ) $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) $(BOOT_OBJ) $(KERNEL_OBJS) -o $@

$(BOOT_OBJ): boot/boot.asm
	@mkdir -p $(@D)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/dlls/%.dll: $(KERNEL_ELF)
	@mkdir -p $(@D)
	@if command -v $(MINGW_CC) >/dev/null 2>&1; then \
		$(MINGW_CC) -shared \
			-o $@ \
			dlls/$*/$*.c \
			-nostdlib -nostartfiles \
			-Wl,--subsystem,windows \
			-Wl,--image-base,0x10000000 \
			-Wl,--entry,_DllMain@12 \
			-Wl,--export-all-symbols \
			$(CPPFLAGS) \
			-L$(BUILD_DIR) \
			-l:kernel.elf; \
	else \
		$(CC) $(CPPFLAGS) -ffreestanding -nostdlib -fno-builtin -m32 -shared -fno-pic \
			-o $@ \
			dlls/$*/$*.c; \
	fi

$(BUILD_DIR)/apps/%.exe: apps/%.c
	@mkdir -p $(@D)
	$(CC) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fno-pic -no-pie -Iwin32 \
		-Wl,-e,main \
		-o $@ \
		$< kernel/util.c

$(BUILD_DIR)/apps/cmd/cmd.exe: apps/cmd/cmd.c
	@mkdir -p $(@D)
	$(CC) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Iwin32 \
		-Wl,-e,main \
		-o $@ \
		$< kernel/util.c

$(SMSS_APP): win32/smss/smss_app.c
	@mkdir -p $(@D)
	$(CC) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fno-pic -no-pie -Iwin32 \
		-Wl,-e,main \
		-o $@ \
		$< kernel/util.c

$(SYSTEM32_DIR)/.stamp: $(DLL_OUTPUTS) $(BUILT_APP_FILES) $(SMSS_APP)
	@mkdir -p $(SYSTEM32_DIR)
	@rm -rf $(ISO_DIR)/APPS
	@for dll in $(DLL_OUTPUTS); do \
		cp "$$dll" "$(SYSTEM32_DIR)/$$(basename "$$dll" | tr '[:lower:]' '[:upper:]')"; \
	done
	@if [ -f "$(SMSS_APP)" ]; then \
		cp "$(SMSS_APP)" "$(SYSTEM32_DIR)/SMSS.EXE"; \
	fi
	@if [ -f "$(BUILD_DIR)/apps/cmd/cmd.exe" ]; then \
		cp "$(BUILD_DIR)/apps/cmd/cmd.exe" "$(SYSTEM32_DIR)/CMD.EXE"; \
	fi
	@touch $@

$(GRUB_DIR)/grub.cfg: boot/grub/grub.cfg $(KERNEL_ELF) | $(SYSTEM32_DIR)/.stamp
	@mkdir -p $(GRUB_DIR) $(ISO_DIR)/boot
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/
	cp $< $@

iso: $(ISO_NAME)

run: $(ISO_NAME)
	qemu-system-i386 -cdrom $(ISO_NAME) -m 64 -vga std -serial stdio

clean:
	rm -rf $(BUILD_DIR) $(ISO_NAME)
