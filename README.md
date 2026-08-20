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
