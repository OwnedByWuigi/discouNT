section .multiboot
align 4
    dd 0x1BADB002              ; Magic
    dd 0x00000007              ; Flags: 1(align) + 2(meminfo) + 4(video)
    dd -(0x1BADB002 + 0x00000007) ; Checksum
    
    dd 0    ; header_addr
    dd 0    ; load_addr  
    dd 0    ; load_end_addr
    dd 0    ; bss_end_addr
    dd 0    ; entry_addr
    
    dd 1    ; mode_type: 1 = graphics mode
    dd 640  ; requested width
    dd 480  ; requested height
    dd 32   ; requested depth

section .text
global start
extern kmain

start:
    mov esp, 0x90000
    push ebx
    push eax
    call kmain
    
    cli
.hang:
    hlt
    jmp .hang
