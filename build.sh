#!/bin/bash
set -e

echo "==> Building NT-like OS with Win32 Subsystem (Step 3)"

# Clean build directory
rm -rf build
mkdir -p build

# Build bootloader
echo "--- Building bootloader ---"
nasm -f elf32 boot/boot.asm -o build/boot.o

# Set up compiler flags
CFLAGS="-ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -m32 -fno-pic -O0 -Ikernel"

echo "--- Compiling kernel core ---"
gcc $CFLAGS -c kernel/util.c -o build/util.o
gcc $CFLAGS -c kernel/mm.c -o build/mm.o
gcc $CFLAGS -c kernel/hal.c -o build/hal.o

echo "--- Compiling Object Manager ---"
gcc $CFLAGS -c kernel/object.c -o build/object.o

echo "--- Compiling Win32k Window Manager ---"
gcc $CFLAGS -c kernel/win32k.c -o build/win32k.o

echo "--- Compiling kernel entry ---"
gcc $CFLAGS -c kernel/entry.c -o build/entry.o

echo "--- Linking kernel ---"
ld -m elf_i386 -T kernel/linker.ld -nostdlib -no-pie \
    build/boot.o \
    build/entry.o \
    build/hal.o \
    build/util.o \
    build/mm.o \
    build/object.o \
    build/win32k.o \
    -o build/kernel.elf

# Check if kernel linked successfully
if [ ! -f build/kernel.elf ]; then
    echo "ERROR: Kernel linking failed!"
    exit 1
fi

echo "--- Verifying multiboot ---"
if command -v grub-file &> /dev/null; then
    if grub-file --is-x86-multiboot build/kernel.elf; then
        echo "Multiboot verification: OK"
    else
        echo "WARNING: Kernel may not be multiboot compliant!"
    fi
fi

echo "--- Creating ISO directory structure ---"
mkdir -p build/iso/boot/grub
cp build/kernel.elf build/iso/boot/kernel.elf

# Create GRUB configuration
cat > build/iso/boot/grub/grub.cfg << 'EOF'
set timeout=0
set default=0

menuentry "NT-like OS (Win32 Test)" {
    multiboot /boot/kernel.elf
    boot
}

menuentry "NT-like OS (Debug Mode)" {
    multiboot /boot/kernel.elf debug
    boot
}
EOF

echo "--- Generating ISO ---"
if command -v grub-mkrescue &> /dev/null; then
    grub-mkrescue -o ntos.iso build/iso 2>/dev/null
elif command -v grub2-mkrescue &> /dev/null; then
    grub2-mkrescue -o ntos.iso build/iso 2>/dev/null
else
    echo "ERROR: grub-mkrescue not found!"
    echo "Install: sudo apt-get install grub-pc-bin xorriso"
    exit 1
fi

# Check ISO size
ISO_SIZE=$(stat -c%s ntos.iso 2>/dev/null || echo 0)
echo "ISO size: $ISO_SIZE bytes"

if [ "$ISO_SIZE" -lt 1000000 ]; then
    echo "WARNING: ISO seems too small, might not boot"
fi

echo ""
echo "=================================="
echo "Build complete!"
echo "=================================="
echo ""
echo "To run:"
echo "  qemu-system-i386 -cdrom ntos.iso -m 64"
echo ""
echo "For debugging:"
echo "  qemu-system-i386 -cdrom ntos.iso -m 64 -d cpu_reset -no-reboot"
echo ""