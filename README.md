<h2>discouNT operating system</h2>

<img src="docs/readme/demo.png" height="500px">
<br>
<i>discouNT in multi-user mode</i>
<br><br>

discouNT is a from-scratch operating system designed to be compatible with Windows NT.

Although it uses some code from WINE and ReactOS, **it is not a fork of either.**

The generated ISO is a hybrid image: it can be used as an optical-disc image or
flashed directly to a USB drive (for example with Etcher, Rufus in DD mode, or
`dd`). The separate `usb-image` target remains available for firmware that only
boots a conventional FAT32 disk.

The GRUB menu includes normal boot, serial debug, and **Screen debug**. Screen
debug displays kernel diagnostics directly in VGA text mode, in the same spirit
as ReactOS's screen debug output.

Storage devices include USB mass-storage disks and legacy IDE/ATA hard drives.
ATA disks are exposed as `Harddisk0` through `Harddisk3`; sector-aligned reads
and writes are supported, and a FAT32 boot volume on `Harddisk0` is mounted
automatically.

The GRUB menu also provides **Install discouNT (Native-mode Setup)**. Setup runs
before the Win32 subsystem, detects IDE/ATA disks, requires an explicit F10
confirmation, and copies the bootable installation image to the selected disk.
