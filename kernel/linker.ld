/* kernel/linker.ld */
ENTRY(kmain)

SECTIONS
{
    . = 0x100000;
    
    .text : {
        *(.text*)
        *(.rodata*)
    }
    
    .data : {
        *(.data*)
    }
    
    .bss : {
        *(COMMON)
        *(.bss*)
    }
    
    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}