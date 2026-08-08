; boot/boot.asm - Request linear framebuffer from GRUB
section .multiboot
align 4
    ; Multiboot header
    dd 0x1BADB002              ; Magic
    dd 0x00000007              ; Flags: align + meminfo + video
    dd -(0x1BADB002 + 0x00000007) ; Checksum
    
    ; Framebuffer requests
    dd 0    ; header_addr
    dd 0    ; load_addr
    dd 0    ; load_end_addr
    dd 0    ; bss_end_addr
    dd 0    ; entry_addr
    dd 1    ; mode_type (1 = graphics mode)
    dd 640  ; width
    dd 480  ; height
    dd 16   ; depth (16 bpp = 5:6:5 RGB)

section .text
global start
extern kmain

start:
    mov esp, 0x90000
    push ebx        ; Multiboot info pointer
    push eax        ; Multiboot magic
    call kmain
    
    cli
.hang:
    hlt
    jmp .hang