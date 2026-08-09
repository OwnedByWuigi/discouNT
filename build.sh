#!/bin/bash
set -e

echo "==> Building NT-like OS"

rm -rf build
mkdir -p build

# Assemble bootloader
echo "--- Bootloader ---"
nasm -f elf32 boot/boot.asm -o build/boot.o

# Compile
CFLAGS="-ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -m32 -fno-pic -O0 -Ikernel"

echo "--- Compiling ---"
FILES=(kernel/util.c kernel/mm.c kernel/hal.c kernel/serial.c
       kernel/object.c kernel/ke.c kernel/vga.c kernel/win32k.c
       kernel/mouse.c kernel/peloader.c kernel/kernel32.c kernel/fb.c kernel/cdfs.c kernel/entry.c)

for src in "${FILES[@]}"; do
    if [ -f "$src" ]; then
        obj="build/$(basename ${src%.c}.o)"
        echo "  CC $src"
        gcc $CFLAGS -c "$src" -o "$obj"
    fi
done

# Build custom DLLs
echo "--- Building DLLs ---"

# Compile each DLL
for dll_dir in dlls/*/; do
    dll_name=$(basename "$dll_dir")
    dll_src="${dll_dir}${dll_name}.c"
    
    if [ -f "$dll_src" ]; then
        echo "  Building $dll_name.dll..."
        
        # Compile as shared library (PE DLL)
        i686-w64-mingw32-gcc -shared \
            -o "build/$dll_name.dll" \
            "$dll_src" \
            -nostdlib -nostartfiles \
            -Wl,--subsystem,windows \
            -Wl,--image-base,0x10000000 \
            -Wl,--entry,_DllMain@12 \
            -Wl,--export-all-symbols \
            -Ikernel \
            -Lbuild \
            -l:kernel.elf 2>/dev/null || true
        
        # If cross-compiler not available, use native gcc
        if [ ! -f "build/$dll_name.dll" ]; then
            gcc -ffreestanding -nostdlib -fno-builtin \
                -m32 -shared -fno-pic \
                -o "build/$dll_name.so" \
                "$dll_src" \
                -Ikernel 2>/dev/null && \
            cp "build/$dll_name.so" "build/$dll_name.dll" || \
            echo "    (skipped - no cross compiler)"
        fi
    fi
done

# Copy DLLs to ISO
mkdir -p build/iso/SYSTEM32
for dll in build/*.dll; do
    if [ -f "$dll" ]; then
        dll_upper=$(basename "$dll" | tr '[:lower:]' '[:upper:]')
        cp "$dll" "build/iso/SYSTEM32/$dll_upper"
        echo "  Added $dll_upper to ISO"
    fi
done

# Link
echo "--- Linking ---"
ld -m elf_i386 -T kernel/linker.ld -nostdlib -no-pie \
    build/boot.o \
    build/entry.o \
    build/hal.o \
    build/util.o \
    build/serial.o \
    build/mm.o \
    build/object.o \
    build/ke.o \
    build/cdfs.o \
    build/vga.o \
    build/peloader.o \
    build/kernel32.o \
    build/fb.o \
    build/win32k.o \
    build/mouse.o \
    -o build/kernel.elf

if [ ! -f build/kernel.elf ]; then
    echo "ERROR: Link failed!"
    exit 1
fi

echo "  Linked successfully"

# Create ISO directory
mkdir -p build/iso/boot/grub
mkdir -p build/iso/APPS
cp build/kernel.elf build/iso/boot/

# Copy EXE files from apps/ folder to ISO
EXE_FOUND=0
for exe in apps/*.exe; do
    if [ -f "$exe" ]; then
        exe_name=$(basename "$exe" | tr '[:lower:]' '[:upper:]')
        cp "$exe" "build/iso/APPS/$exe_name"
        echo "  Added $exe_name to ISO"
        EXE_FOUND=1
    fi
done

if [ $EXE_FOUND -eq 0 ]; then
    echo "  No EXE files found in apps/ - creating test file"
    echo "This is a test file" > build/iso/APPS/README.TXT
fi

cat > build/iso/boot/grub/grub.cfg << 'EOF'
set timeout=0
set default=0

insmod vbe

menuentry "CoreOS" {
    set gfxpayload=800x600x16
    multiboot /boot/kernel.elf
}
EOF

grub-mkrescue -o ntos.iso build/iso 2>/dev/null

echo ""
echo "=================================="
echo "  Build Complete!"
echo "=================================="
echo "  Run: qemu-system-i386 -cdrom ntos.iso -m 64 -vga std -serial stdio"
echo ""