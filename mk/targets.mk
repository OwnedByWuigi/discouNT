.PHONY: all clean iso usb-image kernel dlls apps run run-x86 run-amd64 x86 amd64 loongarch64 run-loongarch64

all: $(ISO_NAME)

kernel: $(KERNEL_ELF)

dlls: $(DLL_OUTPUTS) $(WININET_DLL) $(MSGINA_DLL) $(WIN32K_DLL)

apps: $(BUILT_APP_FILES) $(WAVPLAY_APP) $(CMD_APP) $(CONTROL_APP) $(SMSS_APP) $(CSRSS_APP) $(DESK_CPL) $(TASKMGR_APP) $(NOTEPAD_APP) $(WINVER_APP) $(DXDIAG_APP) $(WHOAMI_APP) $(EXPLORER_APP) $(SC_APP) $(RUNDLL32_APP) $(PROGMAN_APP) $(REGEDIT_APP) $(WINHLP32_APP) $(DRIVER_SYS_FILES) $(WIN32K_DLL)

resources: $(RESOURCE_MENU_OUTPUTS)

RUN_AMD64 := $(filter amd64,$(MAKECMDGOALS))
RUN_X86 := $(filter x86,$(MAKECMDGOALS))
RUN_ISO := $(ISO_NAME)

run-x86:
	$(MAKE) x86
	qemu-system-i386 -cdrom discount-x86.iso -m 128 -vga std -serial stdio -nic user,model=rtl8139 -audiodev $(QEMU_AUDIODEV) -device $(QEMU_SOUND_DEVICE),audiodev=audio0

run-amd64:
	$(MAKE) amd64
	qemu-system-x86_64 -cdrom discount-amd64.iso -m 128 -vga std -serial stdio -nic user,model=rtl8139 -audiodev $(QEMU_AUDIODEV) -device $(QEMU_SOUND_DEVICE),audiodev=audio0

run: run-x86

clean:
	rm -rf $(BUILD_DIR) discount-x86.iso discount-amd64.iso discount-loongarch64.iso
