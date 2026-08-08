ENTRY(start)

SECTIONS
{
    . = 1M;
    
    .multiboot ALIGN(4) : {
        *(.multiboot)
    }
    
    .text ALIGN(4) : {
        *(.text)
    }
    
    .rodata ALIGN(4) : {
        *(.rodata)
    }
    
    .data ALIGN(4) : {
        *(.data)
    }
    
    .bss ALIGN(4) : {
        *(.bss)
        *(COMMON)
    }
}