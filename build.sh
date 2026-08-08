#!/bin/bash
set -e

echo "==> Building NT-like OS ISO"

BOOT_DIR=boot
KERN_DIR=kernel
BUILD_DIR=build
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

# Assemble bootloader
echo "--- Assembling bootloader ---"
nasm -f bin $BOOT_DIR/boot.asm -o $BUILD_DIR/boot.bin

# Compile kernel - add -fno-pic to disable position-independent code
echo "--- Compiling kernel ---"
CFLAGS="-ffreestanding -Wall -Wextra -nostdlib -fno-builtin -fno-exceptions \
        -fno-stack-protector -nostartfiles -m32 -fno-pic -O0 -I$KERN_DIR"

gcc $CFLAGS -c $KERN_DIR/entry.c -o $BUILD_DIR/entry.o
gcc $CFLAGS -c $KERN_DIR/hal.c -o $BUILD_DIR/hal.o
gcc $CFLAGS -c $KERN_DIR/idt.c -o $BUILD_DIR/idt.o
gcc $CFLAGS -c $KERN_DIR/object.c -o $BUILD_DIR/object.o
gcc $CFLAGS -c $KERN_DIR/scheduler.c -o $BUILD_DIR/scheduler.o
gcc $CFLAGS -c $KERN_DIR/util.c -o $BUILD_DIR/util.o

# Assemble ISR stubs with elf32 format
echo "--- Assembling ISR stubs ---"
nasm -f elf32 $KERN_DIR/isr.asm -o $BUILD_DIR/isr.o

# Link kernel - force i386 emulation
echo "--- Linking kernel ---"
ld -m elf_i386 -T $KERN_DIR/linker.ld -o $BUILD_DIR/kernel.elf \
    $BUILD_DIR/entry.o $BUILD_DIR/hal.o $BUILD_DIR/idt.o \
    $BUILD_DIR/object.o $BUILD_DIR/scheduler.o $BUILD_DIR/util.o \
    $BUILD_DIR/isr.o \
    -nostdlib -no-pie --allow-multiple-definition

# Convert to flat binary
echo "--- Creating flat binary ---"
objcopy -O binary $BUILD_DIR/kernel.elf $BUILD_DIR/kernel.bin

# Pad kernel to 64 sectors (32 KiB)
SIZE=$(stat -c%s $BUILD_DIR/kernel.bin)
TARGET=$((64 * 512))
if [ $SIZE -gt $TARGET ]; then
    echo "Error: kernel too large ($SIZE > $TARGET bytes)"
    exit 1
fi
dd if=/dev/zero bs=1 count=$((TARGET - SIZE)) >> $BUILD_DIR/kernel.bin 2>/dev/null

# Create floppy image (1.44 MB)
echo "--- Creating floppy image ---"
dd if=/dev/zero of=$BUILD_DIR/floppy.img bs=512 count=2880 2>/dev/null
dd if=$BUILD_DIR/boot.bin of=$BUILD_DIR/floppy.img conv=notrunc 2>/dev/null
dd if=$BUILD_DIR/kernel.bin of=$BUILD_DIR/floppy.img bs=512 seek=1 conv=notrunc 2>/dev/null

# Generate ISO with El Torito floppy emulation
echo "--- Generating ISO ---"
ISO_DIR=$BUILD_DIR/iso_root
mkdir -p $ISO_DIR
cp $BUILD_DIR/floppy.img $ISO_DIR/ntos.img
xorriso -as mkisofs -R -b ntos.img -no-emul-boot -boot-load-size 4 \
    -boot-info-table -o ntos.iso $ISO_DIR 2>/dev/null

echo "==> Done! ISO image: ntos.iso"
echo "    Run with: qemu-system-i386 -cdrom ntos.iso"