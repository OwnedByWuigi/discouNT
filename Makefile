BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
SYSTEM32_DIR := $(ISO_DIR)/SYSTEM32
GRUB_DIR := $(ISO_DIR)/boot/grub
KERNEL_ISO_PATH := $(SYSTEM32_DIR)/NTOSKRNL.EXE

ISO_NAME := ntos.iso
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
BOOT_OBJ := $(BUILD_DIR)/boot/boot.o

CC := gcc
LD := ld
NASM := nasm
GRUB_MKRESCUE := grub-mkrescue
MINGW_CC := i686-w64-mingw32-gcc

CPPFLAGS := \
	-Iinclude/win32 \
	-Ikernel \
	-Iwin32 \
	-Idrivers/cdfs \
	-Idrivers/fb \
	-Idrivers/keyboard \
	-Idrivers/mouse \
	-Idrivers/net \
	-Idrivers/serial \
	-Idrivers/vga \
	-Iwin32/w32k \
	-Iwin32/smss \
	-Iwin32/csrss

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
	kernel/kexports.c \
	kernel/driver.c \
	kernel/driver_stubs.c \
	kernel/subsystem.c \
	kernel/nativecmd.c \
	win32/csrss/csrss.c \
	win32/smss/smss.c \
	kernel/entry.c

KERNEL_SRCS := $(KERNEL_CORE_SRCS)
KERNEL_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))
BOOT_SERIAL_OBJ := $(BUILD_DIR)/bootdrivers/serial_boot.o
BOOT_CDFS_OBJ := $(BUILD_DIR)/bootdrivers/cdfs_boot.o
KERNEL_EXTRA_OBJS := $(BOOT_SERIAL_OBJ) $(BOOT_CDFS_OBJ)

DLL_DIRS := $(sort $(foreach d,$(wildcard dlls/*),$(if $(wildcard $(d)/$(notdir $(d)).c),$(d),)))
DLL_NAMES := $(notdir $(DLL_DIRS))
DLL_OUTPUTS := $(addprefix $(BUILD_DIR)/dlls/,$(addsuffix .dll,$(DLL_NAMES)))
W32K_DLL := $(BUILD_DIR)/win32/w32k/w32k.dll

APP_SRC_FILES := $(wildcard apps/*.c)
BUILT_APP_FILES := $(patsubst apps/%.c,$(BUILD_DIR)/apps/%.exe,$(APP_SRC_FILES))
SMSS_APP := $(BUILD_DIR)/win32/smss/smss.exe
CSRSS_APP := $(BUILD_DIR)/win32/csrss/csrss.exe
CONTROL_APP := $(BUILD_DIR)/apps/control/control.exe
DESK_CPL := $(BUILD_DIR)/apps/control/desk/desk.cpl
CMD_APP := $(BUILD_DIR)/apps/cmd/cmd.exe
TASKMGR_APP := $(BUILD_DIR)/apps/taskmgr/taskmgr.exe
TASKMGR_SRCS := $(filter %.c,$(wildcard apps/taskmgr/*.c))
NOTEPAD_APP := $(BUILD_DIR)/apps/notepad/notepad.exe
NOTEPAD_SRCS := apps/notepad/main.c apps/notepad/dialog.c
WINVER_APP := $(BUILD_DIR)/apps/winver/winver.exe
WINVER_SRCS := apps/winver/winver.c
ICON_SIDE_CARS := apps/notepad/notepad.ico apps/taskmgr/taskmgr.ico
DRIVERS_DIR := $(SYSTEM32_DIR)/DRIVERS
SERIAL_SYS := $(BUILD_DIR)/drivers/serial/serial.sys
VGA_SYS := $(BUILD_DIR)/drivers/vga/vga.sys
CDFS_SYS := $(BUILD_DIR)/drivers/cdfs/cdfs.sys
KEYBOARD_SYS := $(BUILD_DIR)/drivers/keyboard/keyboard.sys
MOUSE_SYS := $(BUILD_DIR)/drivers/mouse/mouse.sys
NET_SYS := $(BUILD_DIR)/drivers/net/net.sys
FB_SYS := $(BUILD_DIR)/drivers/fb/fb.sys
DRIVER_SYS_FILES := $(SERIAL_SYS) $(VGA_SYS) $(CDFS_SYS) $(KEYBOARD_SYS) $(MOUSE_SYS) $(NET_SYS) $(FB_SYS)

.PHONY: all clean iso kernel dlls apps run run-bridge

all: $(ISO_NAME)

kernel: $(KERNEL_ELF)

dlls: $(DLL_OUTPUTS) $(W32K_DLL)

apps: $(BUILT_APP_FILES) $(CMD_APP) $(CONTROL_APP) $(SMSS_APP) $(CSRSS_APP) $(DESK_CPL) $(TASKMGR_APP) $(NOTEPAD_APP) $(WINVER_APP) $(DRIVER_SYS_FILES) $(W32K_DLL)

$(ISO_NAME): $(SYSTEM32_DIR)/.stamp $(GRUB_DIR)/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(ISO_DIR)

$(KERNEL_ELF): $(BOOT_OBJ) $(KERNEL_OBJS) $(KERNEL_EXTRA_OBJS)
	$(LD) $(LDFLAGS) $(BOOT_OBJ) $(KERNEL_OBJS) $(KERNEL_EXTRA_OBJS) -o $@

$(BOOT_OBJ): boot/boot.asm
	@mkdir -p $(@D)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BOOT_SERIAL_OBJ): drivers/serial/serial.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSerialInit=BootSerialInit -DSerialPutChar=BootSerialPutChar -DSerialPutString=BootSerialPutString -DSerialPrintHex=BootSerialPrintHex -DSerialPrintDec=BootSerialPrintDec -c $< -o $@

$(BOOT_CDFS_OBJ): drivers/cdfs/cdfs.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DCdfsInit=BootCdfsInit -DCdfsReadSector=BootCdfsReadSector -DCdfsFindFile=BootCdfsFindFile -DCdfsReadFile=BootCdfsReadFile -c $< -o $@

define BUILD_DLL_template
$(BUILD_DIR)/dlls/$(1).dll: dlls/$(1)/$(1).c $(KERNEL_ELF)
	@mkdir -p $$(@D)
	@if command -v $(MINGW_CC) >/dev/null 2>&1; then \
		$(MINGW_CC) -shared \
			-o $$@ \
			$$< \
			-nostdlib -nostartfiles \
			-Wl,--subsystem,windows \
			-Wl,--image-base,0x10000000 \
			-Wl,--entry,_DllMain@12 \
			-Wl,--export-all-symbols \
			$(CPPFLAGS) \
			-L$(BUILD_DIR) \
			-l:kernel.elf; \
	else \
		$(CC) $(CPPFLAGS) -ffreestanding -nostdlib -fno-builtin -m32 -fPIC -shared -Wl,-Bsymbolic \
			-o $$@ \
			$$<; \
	fi
endef

$(foreach name,$(DLL_NAMES),$(eval $(call BUILD_DLL_template,$(name))))

$(W32K_DLL): win32/w32k/w32k.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -o $@ $<

$(BUILD_DIR)/apps/%.exe: apps/%.c
	@mkdir -p $(@D)
	$(CC) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fno-pic -no-pie -Iinclude/win32 -Iwin32 \
		-Wl,-e,main \
		-o $@ \
		$< kernel/util.c

$(CMD_APP): apps/cmd/cmd.c
	@mkdir -p $(@D)
	$(CC) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Iinclude/win32 -Iwin32 \
		-Wl,-e,main \
		-o $@ \
		$< kernel/util.c

$(CONTROL_APP): apps/control/control.c
	@mkdir -p $(@D)
	$(CC) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Iinclude/win32 -Iwin32 \
		-Wl,-e,main \
		-o $@ \
		$< kernel/util.c

$(DESK_CPL): apps/control/desk/desk.c
	@mkdir -p $(@D)
	$(CC) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Iwin32 \
		-Wl,-e,main \
		-o $@ \
		$< kernel/util.c

$(TASKMGR_APP): $(TASKMGR_SRCS) kernel/util.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -include string.h -include ctype.h -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,WinMain \
		-o $@ \
		$(TASKMGR_SRCS) kernel/util.c

$(NOTEPAD_APP): $(NOTEPAD_SRCS) kernel/util.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -include string.h -include ctype.h -include stdlib.h -fshort-wchar -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,WinMain \
		-o $@ \
		$(NOTEPAD_SRCS) kernel/util.c

$(WINVER_APP): $(WINVER_SRCS) kernel/util.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -include string.h -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,WinMain \
		-o $@ \
		$(WINVER_SRCS) kernel/util.c

$(SMSS_APP): win32/smss/smss_app.c
	@mkdir -p $(@D)
	$(CC) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fno-pic -no-pie -Iinclude/win32 -Iwin32 \
		-Wl,-e,main \
		-o $@ \
		$< kernel/util.c

$(CSRSS_APP): win32/csrss/csrss_app.c
	@mkdir -p $(@D)
	$(CC) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fno-pic -no-pie -Iinclude/win32 -Iwin32 \
		-Wl,-e,main \
		-o $@ \
		$< kernel/util.c

$(SERIAL_SYS): drivers/serial/serial.c drivers/module_entry.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $^

$(VGA_SYS): drivers/vga/vga.c drivers/module_entry.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $^

$(CDFS_SYS): drivers/cdfs/cdfs.c drivers/module_entry.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $^

$(KEYBOARD_SYS): drivers/keyboard/keyboard.c drivers/module_entry.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $^

$(MOUSE_SYS): drivers/mouse/mouse.c drivers/module_entry.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $^

$(NET_SYS): drivers/net/net.c drivers/module_entry.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $^

$(FB_SYS): drivers/fb/fb.c drivers/module_entry.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $^

$(SYSTEM32_DIR)/.stamp: $(DLL_OUTPUTS) $(BUILT_APP_FILES) $(CMD_APP) $(CONTROL_APP) $(SMSS_APP) $(CSRSS_APP) $(DESK_CPL) $(TASKMGR_APP) $(NOTEPAD_APP) $(WINVER_APP) $(DRIVER_SYS_FILES) $(W32K_DLL) $(KERNEL_ELF)
	@mkdir -p $(SYSTEM32_DIR)
	@mkdir -p $(DRIVERS_DIR)
	@rm -rf $(ISO_DIR)/APPS
	@rm -f "$(DRIVERS_DIR)/WIN32K.SYS"
	@cp "$(KERNEL_ELF)" "$(KERNEL_ISO_PATH)"
	@for dll in $(DLL_OUTPUTS); do \
		cp "$$dll" "$(SYSTEM32_DIR)/$$(basename "$$dll" | tr '[:lower:]' '[:upper:]')"; \
	done
	@if [ -f "$(W32K_DLL)" ]; then \
		cp "$(W32K_DLL)" "$(SYSTEM32_DIR)/WIN32K.DLL"; \
	fi
	@if [ -f "$(SMSS_APP)" ]; then \
		cp "$(SMSS_APP)" "$(SYSTEM32_DIR)/SMSS.EXE"; \
	fi
	@if [ -f "$(CSRSS_APP)" ]; then \
		cp "$(CSRSS_APP)" "$(SYSTEM32_DIR)/CSRSS.EXE"; \
	fi
	@if [ -f "$(CMD_APP)" ]; then \
		cp "$(CMD_APP)" "$(SYSTEM32_DIR)/CMD.EXE"; \
	fi
	@if [ -f "$(CONTROL_APP)" ]; then \
		cp "$(CONTROL_APP)" "$(SYSTEM32_DIR)/CONTROL.EXE"; \
	fi
	@if [ -f "$(DESK_CPL)" ]; then \
		cp "$(DESK_CPL)" "$(SYSTEM32_DIR)/DESK.CPL"; \
	fi
	@if [ -f "$(TASKMGR_APP)" ]; then \
		cp "$(TASKMGR_APP)" "$(SYSTEM32_DIR)/TASKMGR.EXE"; \
	fi
	@if [ -f "apps/taskmgr/taskmgr.ico" ]; then \
		cp "apps/taskmgr/taskmgr.ico" "$(SYSTEM32_DIR)/TASKMGR.ICO"; \
	fi
	@if [ -f "$(NOTEPAD_APP)" ]; then \
		cp "$(NOTEPAD_APP)" "$(SYSTEM32_DIR)/NOTEPAD.EXE"; \
	fi
	@if [ -f "apps/notepad/notepad.ico" ]; then \
		cp "apps/notepad/notepad.ico" "$(SYSTEM32_DIR)/NOTEPAD.ICO"; \
	fi
	@if [ -f "$(WINVER_APP)" ]; then \
		cp "$(WINVER_APP)" "$(SYSTEM32_DIR)/WINVER.EXE"; \
	fi
	@for sys in $(DRIVER_SYS_FILES); do \
		cp "$$sys" "$(DRIVERS_DIR)/$$(basename "$$sys" | tr '[:lower:]' '[:upper:]')"; \
	done
	@touch $@

$(GRUB_DIR)/grub.cfg: boot/grub/grub.cfg $(SYSTEM32_DIR)/.stamp
	@mkdir -p $(GRUB_DIR) $(ISO_DIR)/boot
	cp $< $@

iso: $(ISO_NAME)

run: $(ISO_NAME)
	qemu-system-i386 -cdrom $(ISO_NAME) -m 64 -vga std -serial stdio -nic user,model=rtl8139

run-bridge: $(ISO_NAME)
	qemu-system-i386 -cdrom $(ISO_NAME) -m 64 -vga std -serial stdio -nic tap,model=rtl8139,ifname=tap0,script=no,downscript=no

clean:
	rm -rf $(BUILD_DIR) $(ISO_NAME)
