    .section .text.boot, "ax"
    .global _start
    .type _start, @function

_start:
    /* The virt machine exposes low RAM below its 256 MiB I/O hole. */
    li.d    $sp, 0x001f0000
    bl      boot_main

1:
    idle    0
    b       1b

    .size _start, . - _start
