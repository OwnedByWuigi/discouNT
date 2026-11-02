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
REGEDIT_APP := $(BUILD_DIR)/apps/regedit/regedit.exe
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
$(UMDFHOST_APP): apps/umdfhost.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,main -o $@ $<

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

REGEDIT_SRCS := $(wildcard apps/regedit/*.c)
REGEDIT_MENU_RES := $(BUILD_DIR)/apps/regedit/regedit.menu.bin
$(REGEDIT_APP): $(REGEDIT_SRCS) $(wildcard apps/regedit/*.h) apps/regedit/regedit.rc apps/regedit/regedit_resources.rc include/win32/regedit_api.h include/win32/objsel.h include/win32/ndk/cmtypes.h $(REGEDIT_MENU_RES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Iapps/regedit -include regedit_api.h -include string.h -include stdio.h -include stdlib.h -fshort-wchar -m32 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fPIC -shared -Wl,-Bsymbolic -Wl,-e,wWinMain -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -o $@ $(REGEDIT_SRCS)
	@objcopy --add-section .disres=$(REGEDIT_MENU_RES) --set-section-flags .disres=readonly,data $@

$(REGEDIT_MENU_RES): apps/regedit/regedit_resources.rc apps/regedit/resource.h apps/regedit/lang/en-US.rc tools/rc_menu_gen.py
	@mkdir -p $(@D)
	python3 tools/rc_menu_gen.py $< $@



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

