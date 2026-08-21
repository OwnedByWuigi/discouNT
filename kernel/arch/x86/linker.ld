ENTRY(start)

SECTIONS
{
    . = 1M;
    __kernel_start = .;
    
    .multiboot ALIGN(4K) : {
        *(.multiboot)
    }
    
    .text ALIGN(4K) : {
        *(.text)
    }
    
    .rodata ALIGN(4K) : {
        *(.rodata)
    }
    
    .data ALIGN(4K) : {
        *(.data)
    }
    
    .bss ALIGN(4K) : {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        __bss_end = .;
    }
    __kernel_end = ALIGN(4K);
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
