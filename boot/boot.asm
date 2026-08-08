; boot/boot.asm - Multiboot compliant bootloader
section .multiboot
align 4
    dd 0x1BADB002           ; Magic
    dd 0x00000003           ; Flags (align modules + memory info)
    dd -(0x1BADB002 + 0x00000003)  ; Checksum

section .text
global start
extern kmain

start:
    mov esp, 0x90000        ; Set up stack
    push ebx                ; Push multiboot info pointer
    push eax                ; Push multiboot magic
    call kmain
    
    cli
    hlt
    jmp $