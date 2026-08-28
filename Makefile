BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
SYSTEM32_DIR := $(ISO_DIR)/SYSTEM32
GRUB_DIR := $(ISO_DIR)/boot/grub
KERNEL_ISO_PATH := $(SYSTEM32_DIR)/NTOSKRNL.EXE
WEB_DIR := $(ISO_DIR)/Web
WALLPAPER_FILES := $(wildcard media/wallpaper/*)
WALLPAPER_STAMP := $(WEB_DIR)/.stamp

ISO_NAME := ntos.iso
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
BOOT_OBJ := $(BUILD_DIR)/boot/boot.o

CC := gcc
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

KERNEL_CORE_SRCS := \
	kernel/core/util.c \
	kernel/core/invoke.c \
	kernel/rtl/rtlpath.c \
	kernel/mm/mm.c \
	kernel/mm/pmm.c \
	kernel/mm/vmm.c \
	kernel/arch/x86/hal.c \
	kernel/ob/object.c \
	kernel/io/io.c \
	kernel/io/service.c \
	kernel/core/ke.c \
	kernel/loader/peloader.c \
	kernel/core/kexports.c \
	kernel/io/driver.c \
	kernel/audio/audio_service.c \
	kernel/io/driver_stubs.c \
	kernel/core/setup.c \
	kernel/core/subsystem.c \
	kernel/core/nativecmd.c \
	kernel/core/bugcheck.c \
	kernel/arch/x86/idt.c \
	kernel/arch/x86/isr.c \
	drivers/fat32/fat32.c \
	drivers/fb/storage.c \
	drivers/input/input.c \
	drivers/ide/ide.c \
	drivers/ahci/ahci.c \
	drivers/usb/usb.c \
	drivers/usb/usb_msc.c \
	drivers/usb/uhci.c \
	drivers/usb/ohci.c \
	drivers/usb/ehci.c \
	drivers/usb/xhci.c \
	win32/csrss/csrss_init.c \
	win32/csrss/csrss.c \
	win32/smss/smss_init.c \
	win32/smss/smss.c \
	kernel/core/entry.c

KERNEL_SRCS := $(KERNEL_CORE_SRCS)
KERNEL_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))
BOOT_SERIAL_OBJ := $(BUILD_DIR)/bootdrivers/serial_boot.o
BOOT_CDFS_OBJ := $(BUILD_DIR)/bootdrivers/cdfs_boot.o
ISR_STUBS_OBJ := $(BUILD_DIR)/kernel/isr_stubs.o
KERNEL_EXTRA_OBJS := $(BOOT_SERIAL_OBJ) $(BOOT_CDFS_OBJ) $(ISR_STUBS_OBJ)

DLL_EXCLUDE_DIRS := dlls/user32wine dlls/gdi32wine dlls/msgina
DLL_DIRS := $(sort $(foreach d,$(wildcard dlls/*),$(if $(filter $(d),$(DLL_EXCLUDE_DIRS)),,$(if $(wildcard $(d)/*.c),$(d),))))
DLL_NAMES := $(notdir $(DLL_DIRS))
DLL_OUTPUTS := $(addprefix $(BUILD_DIR)/dlls/,$(addsuffix .dll,$(DLL_NAMES)))
MSGINA_DLL := $(BUILD_DIR)/dlls/msgina.dll
MSGINA_LOGO_OBJ := $(BUILD_DIR)/dlls/msgina/reactos_logo.bmp.o
MSGINA_BAR_OBJ := $(BUILD_DIR)/dlls/msgina/line.bmp.o
WIN32K_DLL := $(BUILD_DIR)/win32/w32k/win32k.dll

APP_SRC_FILES := $(wildcard apps/*.c)
BUILT_APP_FILES := $(patsubst apps/%.c,$(BUILD_DIR)/apps/%.exe,$(APP_SRC_FILES))
WAVPLAY_APP := $(BUILD_DIR)/apps/wavplay.exe
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
DXDIAG_APP := $(BUILD_DIR)/apps/dxdiag/dxdiag.exe
WHOAMI_APP := $(BUILD_DIR)/apps/whoami/whoami.exe
EXPLORER_APP := $(BUILD_DIR)/apps/explorer/explorer.exe
SC_APP := $(BUILD_DIR)/apps/sc/sc.exe
RUNDLL32_APP := $(BUILD_DIR)/apps/rundll32/rundll32.exe
PROGMAN_APP := $(BUILD_DIR)/apps/progman/progman.exe
PROGMAN_SRCS := $(filter %.c,$(wildcard apps/progman/*.c))
PROGMAN_MENU_RES := $(BUILD_DIR)/apps/progman/progman.menu.bin
FLEX ?= flex
WINHLP32_APP := $(BUILD_DIR)/apps/winhlp32/winhlp32.exe
WINHLP32_SRCS := apps/winhlp32/callback.c apps/winhlp32/hlpfile.c \
	apps/winhlp32/macro.c apps/winhlp32/string.c apps/winhlp32/winhelp.c
WINHLP32_LEX := $(BUILD_DIR)/apps/winhlp32/macro.lex.c
WINHLP32_MENU_RES := $(BUILD_DIR)/apps/winhlp32/winhlp32.menu.bin
MAIN_GRP := $(BUILD_DIR)/Main.grp
PROGMAN_INI := apps/progman/progman.ini
EXPLORER_SRCS := $(filter-out %/tests/%,$(wildcard apps/explorer/*.c))
WINVER_SRCS := apps/winver/winver.c
DXDIAG_SRCS := apps/dxdiag/main.c apps/dxdiag/information.c apps/dxdiag/output.c apps/dxdiag/dxdiag_guids.c
RESOURCE_MENU_SRCS := $(wildcard apps/*/*.rc)
RESOURCE_MENU_OUTPUTS := $(patsubst apps/%/%.rc,$(BUILD_DIR)/apps/%/%.menu.bin,$(RESOURCE_MENU_SRCS))
TASKMGR_MENU_RES := $(BUILD_DIR)/apps/taskmgr/taskmgr.menu.bin
NOTEPAD_MENU_RES := $(BUILD_DIR)/apps/notepad/notepad.menu.bin
WINVER_MENU_RES := $(BUILD_DIR)/apps/winver/winver.menu.bin
DRIVERS_DIR := $(SYSTEM32_DIR)/DRIVERS
SERIAL_SYS := $(BUILD_DIR)/drivers/serial/serial.sys
VGA_SYS := $(BUILD_DIR)/drivers/vga/vga.sys
CDFS_SYS := $(BUILD_DIR)/drivers/cdfs/cdfs.sys
KEYBOARD_SYS := $(BUILD_DIR)/drivers/keyboard/keyboard.sys
MOUSE_SYS := $(BUILD_DIR)/drivers/mouse/mouse.sys
NET_SYS := $(BUILD_DIR)/drivers/net/net.sys
FB_SYS := $(BUILD_DIR)/drivers/fb/fb.sys
FONT_DIR := $(SYSTEM32_DIR)/FONTS
FONT_SOURCES := $(wildcard media/fonts/*.ttf)
MEDIA_FILES := $(wildcard media/audio/*)
MEDIA_DIR := $(ISO_DIR)/Media
USB_SYS := $(BUILD_DIR)/drivers/usb/usb.sys
IDE_SYS := $(BUILD_DIR)/drivers/ide/ide.sys
AHCI_SYS := $(BUILD_DIR)/drivers/ahci/ahci.sys
AC97_SYS := $(BUILD_DIR)/drivers/ac97.sys
SB16_SYS := $(BUILD_DIR)/drivers/sb16.sys
ES1371_SYS := $(BUILD_DIR)/drivers/es1371.sys
HDA_SYS := $(BUILD_DIR)/drivers/hda.sys
DRIVER_SYS_FILES := $(SERIAL_SYS) $(VGA_SYS) $(CDFS_SYS) $(KEYBOARD_SYS) $(MOUSE_SYS) $(NET_SYS) $(FB_SYS) $(USB_SYS) $(IDE_SYS) $(AHCI_SYS) $(HDA_SYS) $(ES1371_SYS) $(AC97_SYS) $(SB16_SYS)

.PHONY: all clean iso usb-image kernel dlls apps run run-x86 run-amd64 x86 amd64 loongarch64 run-loongarch64

x86:
	$(MAKE) BUILD_DIR=build/x86 ISO_NAME=ntos-x86.iso all

amd64:
	$(MAKE) -f Makefile.amd64

loongarch64:
	$(MAKE) -f Makefile.loongarch64

run-loongarch64:
	$(MAKE) -f Makefile.loongarch64 run

all: $(ISO_NAME)

kernel: $(KERNEL_ELF)

dlls: $(DLL_OUTPUTS) $(MSGINA_DLL) $(WIN32K_DLL)

apps: $(BUILT_APP_FILES) $(WAVPLAY_APP) $(CMD_APP) $(CONTROL_APP) $(SMSS_APP) $(CSRSS_APP) $(DESK_CPL) $(TASKMGR_APP) $(NOTEPAD_APP) $(WINVER_APP) $(DXDIAG_APP) $(WHOAMI_APP) $(EXPLORER_APP) $(SC_APP) $(RUNDLL32_APP) $(PROGMAN_APP) $(WINHLP32_APP) $(DRIVER_SYS_FILES) $(WIN32K_DLL)

resources: $(RESOURCE_MENU_OUTPUTS)

$(ISO_NAME): $(SYSTEM32_DIR)/.stamp $(GRUB_DIR)/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(ISO_DIR) -volid DISCOUNT

$(KERNEL_ELF): $(BOOT_OBJ) $(KERNEL_OBJS) $(KERNEL_EXTRA_OBJS)
	$(LD) $(LDFLAGS) $(BOOT_OBJ) $(KERNEL_OBJS) $(KERNEL_EXTRA_OBJS) -o $@

$(ISR_STUBS_OBJ): kernel/arch/x86/isr_stubs.asm
	@mkdir -p $(@D)
	$(NASM) -f elf32 $< -o $@

$(BOOT_OBJ): boot/boot.asm
	@mkdir -p $(@D)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BOOT_SERIAL_OBJ): drivers/serial/serial.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSerialInit=BootSerialInit -DSerialSetDebugEnabled=BootSerialSetDebugEnabled -DSerialIsDebugEnabled=BootSerialIsDebugEnabled -DSerialSetScreenDebugEnabled=BootSerialSetScreenDebugEnabled -DSerialIsScreenDebugEnabled=BootSerialIsScreenDebugEnabled -DSerialPutChar=BootSerialPutChar -DSerialPutString=BootSerialPutString -DSerialPrintHex=BootSerialPrintHex -DSerialPrintDec=BootSerialPrintDec -c $< -o $@

$(BOOT_CDFS_OBJ): drivers/cdfs/cdfs.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DCdfsInit=BootCdfsInit -DCdfsReadSector=BootCdfsReadSector -DCdfsFindFile=BootCdfsFindFile -DCdfsReadFile=BootCdfsReadFile -c $< -o $@

define BUILD_DLL_template
DLL_$(1)_SRCS := $$(filter-out %/tests/%,$$(wildcard dlls/$(1)/*.c))
$(BUILD_DIR)/dlls/$(1).dll: $$(DLL_$(1)_SRCS) $(KERNEL_ELF)
	@mkdir -p $$(@D)
	@if command -v $(MINGW_CC) >/dev/null 2>&1; then \
		$(MINGW_CC) -shared \
			-o $$@ \
			$$(DLL_$(1)_SRCS) \
			-nostdlib -nostartfiles \
			-Wl,--subsystem,windows \
			-Wl,--image-base,0x10000000 \
			-Wl,--entry,_DllMain@12 \
			-Wl,--export-all-symbols \
			-Wl,--kill-at \
			$(CPPFLAGS) \
			-L$(BUILD_DIR) \
			-l:kernel.elf; \
	else \
		$(CC) $(CPPFLAGS) -ffreestanding -nostdlib -fno-builtin -m32 -fPIC -shared -Wl,-Bsymbolic \
			-Wl,-e,DllMain \
			-o $$@ \
			$$(DLL_$(1)_SRCS); \
	fi
endef

$(foreach name,$(DLL_NAMES),$(eval $(call BUILD_DLL_template,$(name))))

# These files are included by user32.c rather than compiled as independent
# translation units.  Keep the DLL target aware of them so menu/control
# changes cannot be hidden behind a stale user32.dll.
$(BUILD_DIR)/dlls/user32.dll: dlls/user32/user32_menu.inc dlls/user32/user32_paint.inc dlls/user32/user32_resources.inc

$(MSGINA_LOGO_OBJ): dlls/msgina/resources/reactos.bmp
	@mkdir -p $(@D)
	$(LD) -r -m elf_i386 -b binary -o $@ $<
	@objcopy --redefine-sym _binary_dlls_msgina_resources_reactos_bmp_start=msgina_logo_start $@
	@objcopy --redefine-sym _binary_dlls_msgina_resources_reactos_bmp_end=msgina_logo_end $@

$(MSGINA_BAR_OBJ): dlls/msgina/resources/line.bmp
	@mkdir -p $(@D)
	$(LD) -r -m elf_i386 -b binary -o $@ $<
	@objcopy --redefine-sym _binary_dlls_msgina_resources_line_bmp_start=msgina_bar_start $@
	@objcopy --redefine-sym _binary_dlls_msgina_resources_line_bmp_end=msgina_bar_end $@

$(MSGINA_DLL): dlls/msgina/msgina.c dlls/msgina/gui.c dlls/msgina/compat/reactos_port.c dlls/msgina/compat/ui_port.c include/win32/discount_dialog.h dlls/msgina/resources/reactos.bmp dlls/msgina/resources/line.bmp $(MSGINA_LOGO_OBJ) $(MSGINA_BAR_OBJ) $(KERNEL_ELF)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles \
		-Idlls/msgina/compat -Idlls/msgina \
		-fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,--export-dynamic -o $@ \
		dlls/msgina/msgina.c \
		dlls/msgina/compat/reactos_port.c \
		dlls/msgina/gui.c \
		dlls/msgina/compat/ui_port.c \
		$(MSGINA_LOGO_OBJ) \
		$(MSGINA_BAR_OBJ)
	@objcopy --add-section .disbmp_logo=dlls/msgina/resources/reactos.bmp --set-section-flags .disbmp_logo=readonly,data $@
	@objcopy --add-section .disbmp_bar=dlls/msgina/resources/line.bmp --set-section-flags .disbmp_bar=readonly,data $@

$(WIN32K_DLL): win32/w32k/w32k.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -o $@ $<

$(BUILD_DIR)/apps/%.exe: apps/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fno-pic -no-pie \
		-Wl,-e,main \
		-o $@ \
		$< kernel/core/util.c

$(CMD_APP): apps/cmd/cmd.c kernel/core/version.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,main \
		-o $@ \
		$< kernel/core/util.c

$(WAVPLAY_APP): apps/wavplay.c apps/audio/sound.c apps/audio/sound.h drivers/audio/audio.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,main -o $@ apps/wavplay.c apps/audio/sound.c

$(CONTROL_APP): apps/control/control.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,main \
		-o $@ \
		$< kernel/core/util.c

$(DESK_CPL): apps/control/desk/desk.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,main \
		-o $@ \
		$< kernel/core/util.c

$(TASKMGR_APP): $(TASKMGR_SRCS) kernel/core/util.c $(TASKMGR_MENU_RES) apps/taskmgr/taskmgr.ico include/win32/commctrl.h include/win32/windows.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -include string.h -include ctype.h -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,WinMain \
		-o $@ \
		$(TASKMGR_SRCS) kernel/core/util.c
	@objcopy --add-section .disres=$(TASKMGR_MENU_RES) --set-section-flags .disres=readonly,data $@
	@objcopy --add-section .disicon=apps/taskmgr/taskmgr.ico --set-section-flags .disicon=readonly,data $@

$(NOTEPAD_APP): $(NOTEPAD_SRCS) kernel/core/util.c $(NOTEPAD_MENU_RES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -include string.h -include ctype.h -include stdlib.h -fshort-wchar -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,WinMain \
		-o $@ \
		$(NOTEPAD_SRCS) kernel/core/util.c
	@objcopy --add-section .disres=$(NOTEPAD_MENU_RES) --set-section-flags .disres=readonly,data $@

$(WINVER_APP): $(WINVER_SRCS) kernel/core/util.c $(WINVER_MENU_RES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -include string.h -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,WinMain \
		-o $@ \
		$(WINVER_SRCS) kernel/core/util.c
	@objcopy --add-section .disres=$(WINVER_MENU_RES) --set-section-flags .disres=readonly,data $@

$(DXDIAG_APP): $(DXDIAG_SRCS) apps/dxdiag/dxdiag_private.h include/win32/dxdiag.h include/win32/msxml2.h include/win32/initguid.h include/win32/wine/debug.h kernel/core/util.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Iapps/dxdiag -include string.h -include stdio.h -include stdlib.h -fshort-wchar -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,wWinMain -o $@ $(DXDIAG_SRCS) kernel/core/util.c

$(WHOAMI_APP): apps/whoami/main.c apps/whoami/compat.c apps/whoami/entry.c kernel/core/util.c include/win32/security.h include/win32/sddl.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -fshort-wchar -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,main -o $@ \
		apps/whoami/main.c apps/whoami/compat.c apps/whoami/entry.c kernel/core/util.c

$(EXPLORER_APP): $(EXPLORER_SRCS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Iapps/explorer -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,wWinMain -o $@ $(EXPLORER_SRCS)

$(RUNDLL32_APP): apps/rundll32/rundll32.c include/win32/windows.h include/win32/string.h include/win32/stdlib.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,wWinMain -o $@ $<

$(PROGMAN_APP): $(PROGMAN_SRCS) apps/progman/progman.h apps/progman/progman.rc include/win32/windows.h include/win32/commdlg.h include/win32/mmsystem.h $(PROGMAN_MENU_RES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Iapps/progman -include string.h -include stdio.h -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,WinMain -o $@ $(PROGMAN_SRCS)
	@objcopy --add-section .disres=$(PROGMAN_MENU_RES) --set-section-flags .disres=readonly,data $@

$(SC_APP): apps/sc/sc.c include/win32/winsvc.h include/win32/windows.h dlls/kernel32/kernel32.c dlls/advapi32/advapi32.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -include stdio.h -include string.h -include stdlib.h -fshort-wchar -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic \
		-Wl,-e,wmain -o $@ apps/sc/sc.c

$(TASKMGR_MENU_RES): apps/taskmgr/taskmgr.rc tools/rc_menu_gen.py
	@mkdir -p $(@D)
	python3 tools/rc_menu_gen.py $< $@

$(NOTEPAD_MENU_RES): apps/notepad/notepad.rc tools/rc_menu_gen.py
	@mkdir -p $(@D)
	python3 tools/rc_menu_gen.py $< $@

$(WINVER_MENU_RES): apps/winver/winver.rc tools/rc_menu_gen.py
	@mkdir -p $(@D)
	python3 tools/rc_menu_gen.py $< $@

$(PROGMAN_MENU_RES): apps/progman/progman.rc apps/progman/progman.h tools/rc_menu_gen.py
	@mkdir -p $(@D)
	python3 tools/rc_menu_gen.py $< $@

$(WINHLP32_LEX): apps/winhlp32/macro.lex.l
	@mkdir -p $(@D)
	$(FLEX) -o $@ $<

$(WINHLP32_MENU_RES): apps/winhlp32/winhlp32.rc apps/winhlp32/winhelp_res.h tools/rc_menu_gen.py
	@mkdir -p $(@D)
	python3 tools/rc_menu_gen.py $< $@

$(WINHLP32_APP): $(WINHLP32_SRCS) $(WINHLP32_LEX) apps/winhlp32/winhelp.h apps/winhlp32/hlpfile.h apps/winhlp32/macro.h apps/winhlp32/winhlp32.rc apps/winhlp32/winhelp.ico $(WINHLP32_MENU_RES)
	@mkdir -p $(@D)
		$(CC) $(CPPFLAGS) -Iapps/winhlp32 -include windows.h -include string.h -include stdio.h -include stdlib.h -fshort-wchar -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,WinMain -o $@ $(WINHLP32_SRCS) $(WINHLP32_LEX)
	@objcopy --add-section .disres=$(WINHLP32_MENU_RES) --set-section-flags .disres=readonly,data $@
	@objcopy --add-section .disicon=apps/winhlp32/winhelp.ico --set-section-flags .disicon=readonly,data $@

$(SMSS_APP): win32/smss/smss_app.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fno-pic -no-pie \
		-Wl,-e,main \
		-o $@ \
		$< kernel/core/util.c

$(CSRSS_APP): win32/csrss/csrss_app.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fno-pic -no-pie \
		-Wl,-e,main \
		-o $@ \
		$< kernel/core/util.c

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
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ drivers/fb/fb.c drivers/fb/ttf.c drivers/module_entry.c

USB_SOURCES := drivers/usb/usb.c drivers/usb/usb_msc.c drivers/usb/uhci.c drivers/usb/ohci.c drivers/usb/ehci.c drivers/usb/xhci.c

$(USB_SYS): $(USB_SOURCES) drivers/usb/usb.h drivers/usb/usb_internal.h drivers/usb/usb_msc.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $(USB_SOURCES)

$(IDE_SYS): drivers/ide/ide.c drivers/ide/ide.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ drivers/ide/ide.c

$(AHCI_SYS): drivers/ahci/ahci.c drivers/ahci/ahci.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ drivers/ahci/ahci.c

$(AC97_SYS): drivers/audio/ac97.c drivers/audio/audio.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $<

$(ES1371_SYS): drivers/audio/es1371.c drivers/audio/audio.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $<

$(HDA_SYS): drivers/audio/hda.c drivers/audio/audio.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $<

$(SB16_SYS): drivers/audio/sb16.c drivers/audio/audio.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fPIC -shared -Wl,-Bsymbolic -Wl,-e,DriverEntry -o $@ $<

$(WALLPAPER_STAMP): $(WALLPAPER_FILES) tools/prepare_wallpapers.py
	@mkdir -p "$(WEB_DIR)"
	python3 tools/prepare_wallpapers.py "$(WEB_DIR)" $(WALLPAPER_FILES)

$(SYSTEM32_DIR)/.stamp: $(DLL_OUTPUTS) $(MSGINA_DLL) $(BUILT_APP_FILES) $(WAVPLAY_APP) $(CMD_APP) $(CONTROL_APP) $(SMSS_APP) $(CSRSS_APP) $(DESK_CPL) $(TASKMGR_APP) $(NOTEPAD_APP) $(WINVER_APP) $(DXDIAG_APP) $(WHOAMI_APP) $(EXPLORER_APP) $(SC_APP) $(RUNDLL32_APP) $(PROGMAN_APP) $(WINHLP32_APP) $(RESOURCE_MENU_OUTPUTS) $(DRIVER_SYS_FILES) $(WIN32K_DLL) $(KERNEL_ELF) $(FONT_SOURCES) $(MEDIA_FILES) $(MAIN_GRP) $(PROGMAN_INI) $(WALLPAPER_STAMP)
	@mkdir -p $(SYSTEM32_DIR)
	@cp "$(MAIN_GRP)" "$(ISO_DIR)/Main.grp"
	@cp "$(MAIN_GRP)" "$(SYSTEM32_DIR)/Main.grp"
	@cp "$(PROGMAN_INI)" "$(ISO_DIR)/progman.ini"
	@cp "$(PROGMAN_INI)" "$(SYSTEM32_DIR)/progman.ini"
	@mkdir -p $(DRIVERS_DIR)
	@mkdir -p $(FONT_DIR)
	@for font in $(FONT_SOURCES); do cp "$$font" "$(FONT_DIR)/$$(basename "$$font" | tr '[:lower:]' '[:upper:]')"; done
	@rm -rf $(ISO_DIR)/APPS
	@rm -f "$(DRIVERS_DIR)/WIN32K.SYS"
	@cp "$(KERNEL_ELF)" "$(KERNEL_ISO_PATH)"
	@for dll in $(DLL_OUTPUTS); do \
		cp "$$dll" "$(SYSTEM32_DIR)/$$(basename "$$dll" | tr '[:lower:]' '[:upper:]')"; \
	done
	@cp "$(MSGINA_DLL)" "$(SYSTEM32_DIR)/MSGINA.DLL"
	@rm -f "$(SYSTEM32_DIR)/W32K.DLL"
	@if [ -f "$(WIN32K_DLL)" ]; then \
		cp "$(WIN32K_DLL)" "$(SYSTEM32_DIR)/WIN32K.DLL"; \
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
	@if [ -f "$(WAVPLAY_APP)" ]; then cp "$(WAVPLAY_APP)" "$(SYSTEM32_DIR)/WAVPLAY.EXE"; fi
	@mkdir -p "$(MEDIA_DIR)"
	@for audio in $(MEDIA_FILES); do cp "$$audio" "$(MEDIA_DIR)/$$(basename "$$audio")"; done
	@if [ -f "$(CONTROL_APP)" ]; then \
		cp "$(CONTROL_APP)" "$(SYSTEM32_DIR)/CONTROL.EXE"; \
	fi
	@if [ -f "$(DESK_CPL)" ]; then \
		cp "$(DESK_CPL)" "$(SYSTEM32_DIR)/DESK.CPL"; \
	fi
	@if [ -f "$(TASKMGR_APP)" ]; then \
		cp "$(TASKMGR_APP)" "$(SYSTEM32_DIR)/TASKMGR.EXE"; \
	fi
	@rm -f "$(SYSTEM32_DIR)/TASKMGR.ICO" "$(SYSTEM32_DIR)/NOTEPAD.ICO"
	@if [ -f "$(NOTEPAD_APP)" ]; then \
		cp "$(NOTEPAD_APP)" "$(SYSTEM32_DIR)/NOTEPAD.EXE"; \
	fi
	@if [ -f "$(WINVER_APP)" ]; then \
		cp "$(WINVER_APP)" "$(SYSTEM32_DIR)/WINVER.EXE"; \
	fi
	@if [ -f "$(DXDIAG_APP)" ]; then \
		cp "$(DXDIAG_APP)" "$(SYSTEM32_DIR)/DXDIAG.EXE"; \
	fi
	@if [ -f "$(WHOAMI_APP)" ]; then \
		cp "$(WHOAMI_APP)" "$(SYSTEM32_DIR)/WHOAMI.EXE"; \
	fi
	@if [ -f "$(EXPLORER_APP)" ]; then \
		cp "$(EXPLORER_APP)" "$(SYSTEM32_DIR)/EXPLORER.EXE"; \
	fi
	@if [ -f "$(SC_APP)" ]; then \
		cp "$(SC_APP)" "$(SYSTEM32_DIR)/SC.EXE"; \
	fi
	@if [ -f "$(RUNDLL32_APP)" ]; then \
		cp "$(RUNDLL32_APP)" "$(SYSTEM32_DIR)/RUNDLL32.EXE"; \
	fi
	@if [ -f "$(PROGMAN_APP)" ]; then \
		cp "$(PROGMAN_APP)" "$(SYSTEM32_DIR)/PROGMAN.EXE"; \
	fi
	@if [ -f "$(WINHLP32_APP)" ]; then \
		cp "$(WINHLP32_APP)" "$(SYSTEM32_DIR)/WINHLP32.EXE"; \
	fi
	@for sys in $(DRIVER_SYS_FILES); do \
		cp "$$sys" "$(DRIVERS_DIR)/$$(basename "$$sys" | tr '[:lower:]' '[:upper:]')"; \
	done
	@touch $@

$(MAIN_GRP): tools/make_main_grp.py
	@mkdir -p $(@D)
	python3 $< $@


$(GRUB_DIR)/grub.cfg: boot/grub/grub.cfg $(SYSTEM32_DIR)/.stamp
	@mkdir -p $(GRUB_DIR) $(ISO_DIR)/boot
	cp $< $@

iso: $(ISO_NAME)

usb-image: $(SYSTEM32_DIR)/.stamp $(GRUB_DIR)/grub.cfg
	sh tools/make_usb_image.sh $(ISO_DIR) $(GRUB_DIR)/grub.cfg ntos-usb.img

RUN_AMD64 := $(filter amd64,$(MAKECMDGOALS))
RUN_X86 := $(filter x86,$(MAKECMDGOALS))
RUN_ISO := $(if $(RUN_AMD64),ntos-amd64.iso,$(if $(RUN_X86),ntos-x86.iso,$(ISO_NAME)))

run-x86:
	$(MAKE) x86
	qemu-system-i386 -cdrom ntos-x86.iso -m 128 -vga std -serial stdio -nic user,model=rtl8139 -audiodev $(QEMU_AUDIODEV) -device $(QEMU_SOUND_DEVICE),audiodev=audio0

run-amd64:
	$(MAKE) amd64
	qemu-system-x86_64 -cdrom ntos-amd64.iso -m 128 -vga std -serial stdio -nic user,model=rtl8139 -audiodev $(QEMU_AUDIODEV) -device $(QEMU_SOUND_DEVICE),audiodev=audio0

run: run-x86

clean:
	rm -rf $(BUILD_DIR) ntos-x86.iso ntos-amd64.iso
