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
MEDIA_DIR := $(DISCOUNT_DIR)/Media
REGISTRY_HIVES := $(wildcard system/registry/*)
USB_SYS := $(BUILD_DIR)/drivers/usb/usb.sys
IDE_SYS := $(BUILD_DIR)/drivers/ide/ide.sys
AHCI_SYS := $(BUILD_DIR)/drivers/ahci/ahci.sys
AC97_SYS := $(BUILD_DIR)/drivers/ac97.sys
SB16_SYS := $(BUILD_DIR)/drivers/sb16.sys
ES1371_SYS := $(BUILD_DIR)/drivers/es1371.sys
HDA_SYS := $(BUILD_DIR)/drivers/hda.sys
DRIVER_SYS_FILES := $(SERIAL_SYS) $(VGA_SYS) $(CDFS_SYS) $(KEYBOARD_SYS) $(MOUSE_SYS) $(NET_SYS) $(FB_SYS) $(USB_SYS) $(IDE_SYS) $(AHCI_SYS) $(HDA_SYS) $(ES1371_SYS) $(AC97_SYS) $(SB16_SYS)
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

