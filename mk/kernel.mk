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
	kernel/io/wudf_transport.c \
	compat/jpeg/picojpeg.c \
	compat/jpeg/jpeg_image.c \
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
	compat/wdf/platform/src/wdf_platform.c \
	compat/wdf/platform/src/wdf_core.c \
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

$(ISO_NAME): $(SYSTEM32_DIR)/.stamp $(GRUB_DIR)/grub.cfg
	@rm -rf "$(ISO_DIR)/SYSTEM32" "$(ISO_DIR)/Media" "$(ISO_DIR)/Web" "$(ISO_DIR)/APPS" "$(ISO_DIR)/Main.grp" "$(ISO_DIR)/progman.ini"
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
	$(CC) $(CPPFLAGS) $(WDF_PLATFORM_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BOOT_SERIAL_OBJ): drivers/serial/serial.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSerialInit=BootSerialInit -DSerialSetDebugEnabled=BootSerialSetDebugEnabled -DSerialIsDebugEnabled=BootSerialIsDebugEnabled -DSerialSetScreenDebugEnabled=BootSerialSetScreenDebugEnabled -DSerialIsScreenDebugEnabled=BootSerialIsScreenDebugEnabled -DSerialPutChar=BootSerialPutChar -DSerialPutString=BootSerialPutString -DSerialPrintHex=BootSerialPrintHex -DSerialPrintDec=BootSerialPrintDec -c $< -o $@

$(BOOT_CDFS_OBJ): drivers/cdfs/cdfs.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DCdfsInit=BootCdfsInit -DCdfsReadSector=BootCdfsReadSector -DCdfsFindFile=BootCdfsFindFile -DCdfsReadFile=BootCdfsReadFile -c $< -o $@

