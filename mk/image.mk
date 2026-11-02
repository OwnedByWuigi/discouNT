$(WALLPAPER_STAMP): $(WALLPAPER_FILES) tools/prepare_wallpapers.py
	@mkdir -p "$(WEB_DIR)"
	python3 tools/prepare_wallpapers.py "$(WEB_DIR)" $(WALLPAPER_FILES)

$(SYSTEM32_DIR)/.stamp: $(DLL_OUTPUTS) $(WININET_DLL) $(MSGINA_DLL) $(BUILT_APP_FILES) $(UMDFHOST_APP) $(WAVPLAY_APP) $(CMD_APP) $(CONTROL_APP) $(SMSS_APP) $(CSRSS_APP) $(DESK_CPL) $(TASKMGR_APP) $(NOTEPAD_APP) $(WINVER_APP) $(DXDIAG_APP) $(WHOAMI_APP) $(EXPLORER_APP) $(SC_APP) $(RUNDLL32_APP) $(PROGMAN_APP) $(REGEDIT_APP) $(WINHLP32_APP) $(RESOURCE_MENU_OUTPUTS) $(DRIVER_SYS_FILES) $(WIN32K_DLL) $(KERNEL_ELF) $(FONT_SOURCES) $(MEDIA_FILES) $(REGISTRY_HIVES) $(MAIN_GRP) $(PROGMAN_INI) $(WALLPAPER_STAMP)
	@mkdir -p $(SYSTEM32_DIR)
	@cp "$(MAIN_GRP)" "$(SYSTEM32_DIR)/Main.grp"
	@cp "$(PROGMAN_INI)" "$(SYSTEM32_DIR)/progman.ini"
	@mkdir -p $(DRIVERS_DIR)
	@mkdir -p $(FONT_DIR)
	@mkdir -p $(SYSTEM32_DIR)/CONFIG
	@for hive in $(REGISTRY_HIVES); do cp "$$hive" "$(SYSTEM32_DIR)/CONFIG/$$(basename "$$hive" | tr '[:lower:]' '[:upper:]')"; done
	@for font in $(FONT_SOURCES); do cp "$$font" "$(FONT_DIR)/$$(basename "$$font" | tr '[:lower:]' '[:upper:]')"; done
	@rm -rf $(ISO_DIR)/APPS
	@rm -f "$(DRIVERS_DIR)/WIN32K.SYS"
	@cp "$(KERNEL_ELF)" "$(KERNEL_ISO_PATH)"
	@for dll in $(DLL_OUTPUTS) $(WININET_DLL); do \
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
	@if [ -f "$(UMDFHOST_APP)" ]; then cp "$(UMDFHOST_APP)" "$(SYSTEM32_DIR)/UMDFHOST.EXE"; fi
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
	@if [ -f "$(REGEDIT_APP)" ]; then cp "$(REGEDIT_APP)" "$(SYSTEM32_DIR)/REGEDIT.EXE"; fi
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

