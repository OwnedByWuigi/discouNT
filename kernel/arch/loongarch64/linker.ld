ENTRY(_start)

SECTIONS {
    /* QEMU virt reset ROM window. */
    . = 0x1c000000;

    .text : ALIGN(16) {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
        *(.got .got.*)
    }

    .data 0x00100000 : AT(ALIGN(LOADADDR(.rodata) + SIZEOF(.rodata), 16)) {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
