ENTRY(start64)
SECTIONS
{
    . = 1M;
    .multiboot ALIGN(4K) : { *(.multiboot) }
    .text ALIGN(4K) : { *(.text) }
    .rodata ALIGN(4K) : { *(.rodata) }
    .data ALIGN(4K) : { *(.data) }
    .bss ALIGN(4K) : { __bss_start = .; *(COMMON) *(.bss) __bss_end = .; }
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note.gnu.build-id) }
}
