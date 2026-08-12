BITS 32
section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00000007
    dd -(0x1BADB002 + 0x00000007)
    dd 0, 0, 0, 0, 0
    dd 1, 640, 480, 32
section .text
global start64
extern kmain
extern __pml4
extern __pdpt
extern __pd
start64:
    cli
    mov [boot_magic], eax
    mov [boot_info], ebx
    mov esp, stack64_top
    mov eax, __pdpt
    or eax, 3
    mov [__pml4], eax
    mov eax, __pd
    or eax, 3
    mov [__pdpt], eax
    xor ecx, ecx
.map:
    mov eax, ecx
    shl eax, 21
    or eax, 0x83
    mov [__pd + ecx * 8], eax
    mov dword [__pd + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jb .map
    mov eax, __pml4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    lgdt [gdt64_ptr]
    jmp 0x08:long_mode
BITS 64
long_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov edi, [boot_magic]
    mov esi, [boot_info]
    call kmain
.hang:
    cli
    hlt
    jmp .hang
section .data
align 8
gdt64:
    dq 0
    dq 0x00AF9A000000FFFF
    dq 0x00AF92000000FFFF
gdt64_ptr:
    dw gdt64_ptr - gdt64 - 1
    dq gdt64
boot_magic: dd 0
boot_info: dd 0
section .bss
align 4096
__pml4: resq 512
__pdpt: resq 512
__pd: resq 512
align 16
stack64: resb 65536
stack64_top:
