#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 STAGING_DIRECTORY GRUB_CONFIG OUTPUT_IMAGE" >&2
    exit 2
fi

staging=$1
config=$2
output=$3
efi_image="${output}.bootx64.efi"

for tool in mformat mcopy mmd grub-mkstandalone truncate; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

rm -f "$output" "$efi_image"
truncate -s 256M "$output"
mformat -i "$output" -F -v DISCOUNT ::
grub-mkstandalone -O x86_64-efi -o "$efi_image" \
    "boot/grub/grub.cfg=$config"
mcopy -i "$output" -s "$staging"/* ::
mmd -i "$output" ::/EFI ::/EFI/BOOT
mcopy -i "$output" "$efi_image" ::/EFI/BOOT/BOOTX64.EFI
rm -f "$efi_image"
echo "Created bootable FAT32 USB image: $output"
