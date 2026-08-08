#!/bin/bash
set -e

echo "==> Building VGA Test"

rm -rf build
mkdir -p build

nasm -f elf32 boot/boot.asm -o build/boot.o

CFLAGS="-ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -m32 -fno-pic -O0 -Ikernel"

gcc $CFLAGS -c kernel/util.c -o build/util.o
gcc $CFLAGS -c kernel/mm.c -o build/mm.o
gcc $CFLAGS -c kernel/hal.c -o build/hal.o
gcc $CFLAGS -c kernel/vga.c -o build/vga.o
gcc $CFLAGS -c kernel/entry.c -o build/entry.o

ld -m elf_i386 -T kernel/linker.ld -nostdlib -no-pie \
    build/boot.o build/entry.o build/hal.o build/util.o \
    build/mm.o build/vga.o \
    -o build/kernel.elf

mkdir -p build/iso/boot/grub
cp build/kernel.elf build/iso/boot/

cat > build/iso/boot/grub/grub.cfg << 'EOF'
set timeout=0
set default=0
menuentry "VGA Test" {
    multiboot /boot/kernel.elf
}
EOF

grub-mkrescue -o test.iso build/iso 2>/dev/null
echo "==> Done! Run: qemu-system-i386 -cdrom test.iso -m 64"