#!/bin/bash
set -e

echo "==> Building NT-like OS"

rm -rf build
mkdir -p build

# Assemble bootloader only
echo "--- Bootloader ---"
nasm -f elf32 boot/boot.asm -o build/boot.o

# Compile
CFLAGS="-ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -m32 -fno-pic -O0 -Ikernel"

echo "--- Compiling ---"
FILES=(kernel/util.c kernel/mm.c kernel/hal.c kernel/serial.c
       kernel/object.c kernel/ke.c kernel/vga.c kernel/win32k.c
       kernel/mouse.c kernel/entry.c)

for src in "${FILES[@]}"; do
    if [ -f "$src" ]; then
        obj="build/$(basename ${src%.c}.o)"
        echo "  CC $src"
        gcc $CFLAGS -c "$src" -o "$obj"
    fi
done

# Link - NO isr_stubs.o
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
    build/vga.o \
    build/win32k.o \
    build/mouse.o \
    -o build/kernel.elf

if [ ! -f build/kernel.elf ]; then
    echo "ERROR: Link failed!"
    exit 1
fi

echo "  Linked successfully"

# ISO
mkdir -p build/iso/boot/grub
cp build/kernel.elf build/iso/boot/

cat > build/iso/boot/grub/grub.cfg << 'EOF'
set timeout=0
set default=0
menuentry "NT-like OS" {
    multiboot /boot/kernel.elf
}
EOF

grub-mkrescue -o ntos.iso build/iso 2>/dev/null

echo ""
echo "=================================="
echo "  Build Complete!"
echo "=================================="
echo "  Run: qemu-system-i386 -cdrom ntos.iso -m 64 -serial stdio"