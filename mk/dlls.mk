DLL_EXCLUDE_DIRS := dlls/user32wine dlls/gdi32wine dlls/msgina dlls/wininet
DLL_DIRS := $(sort $(foreach d,$(wildcard dlls/*),$(if $(filter $(d),$(DLL_EXCLUDE_DIRS)),,$(if $(wildcard $(d)/*.c),$(d),))))
DLL_NAMES := $(notdir $(DLL_DIRS))
DLL_OUTPUTS := $(addprefix $(BUILD_DIR)/dlls/,$(addsuffix .dll,$(DLL_NAMES)))
WININET_DLL := $(BUILD_DIR)/dlls/wininet.dll
MSGINA_DLL := $(BUILD_DIR)/dlls/msgina.dll
MSGINA_LOGO_OBJ := $(BUILD_DIR)/dlls/msgina/reactos_logo.bmp.o
MSGINA_BAR_OBJ := $(BUILD_DIR)/dlls/msgina/line.bmp.o
WIN32K_DLL := $(BUILD_DIR)/win32/w32k/win32k.dll

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
		$(CC) $(CPPFLAGS) -fshort-wchar -ffreestanding -nostdlib -fno-builtin -m32 -fPIC -shared -Wl,-Bsymbolic \
			-Wl,-e,DllMain \
			-o $$@ \
			$$(DLL_$(1)_SRCS); \
	fi
endef

$(foreach name,$(DLL_NAMES),$(eval $(call BUILD_DLL_template,$(name))))

$(WININET_DLL): dlls/wininet/wininet_native.c include/win32/wininet.h $(KERNEL_ELF)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin \
		-fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -o $@ $<

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
	$(CC) $(CPPFLAGS) -fshort-wchar -m32 -ffreestanding -nostdlib -nostartfiles \
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

