; boot/boot.asm
; Boot sector – loads kernel from disk using BIOS, enables A20, enters protected mode

BITS 16
ORG 0x7C00

KERNEL_LOAD_SEG  equ 0x1000   ; Load kernel at 0x10000 (segment address)
KERNEL_SECTORS   equ 64       ; 64 sectors = 32 KiB kernel image
KERNEL_TARGET    equ 0x100000 ; Final destination for kernel (1 MiB)

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Save boot drive number
    mov [boot_drive], dl

    ; Display loading message
    mov si, loading_msg
    call print_string

    ; Enable A20 line using keyboard controller
    call enable_a20

    ; Load kernel sectors from disk
    mov ah, 0x02         ; BIOS read sectors function
    mov al, KERNEL_SECTORS
    mov ch, 0            ; Cylinder 0
    mov cl, 2            ; Sector 2 (sector 1 is boot sector)
    mov dh, 0            ; Head 0
    mov dl, [boot_drive] ; Drive number
    mov bx, KERNEL_LOAD_SEG
    mov es, bx           ; ES:BX = 0x1000:0x0000
    mov bx, 0
    int 0x13
    jc disk_error

    ; Copy kernel from 0x10000 to 0x100000
    mov si, load_ok_msg
    call print_string

    ; Disable interrupts for mode switch
    cli

    ; Load GDTR
    lgdt [gdt_desc]

    ; Enter protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to clear prefetch queue and enter 32-bit mode
    jmp 0x08:protected_mode

disk_error:
    mov si, err_msg
    call print_string
    mov ah, 0
    int 0x16            ; Wait for keypress
    int 0x19            ; Reboot
    jmp $

print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string
.done:
    ret

enable_a20:
    ; Try BIOS method first
    mov ax, 0x2401
    int 0x15
    jnc .done
    
    ; Fallback: keyboard controller method
    cli
    call .wait_input
    mov al, 0xAD        ; Disable keyboard
    out 0x64, al
    call .wait_input
    mov al, 0xD0        ; Read output port
    out 0x64, al
    call .wait_output
    in al, 0x60
    push ax
    call .wait_input
    mov al, 0xD1        ; Write output port
    out 0x64, al
    call .wait_input
    pop ax
    or al, 2            ; Enable A20 bit
    out 0x60, al
    call .wait_input
    mov al, 0xAE        ; Enable keyboard
    out 0x64, al
    call .wait_input
.done:
    ret

.wait_input:
    in al, 0x64
    test al, 2
    jnz .wait_input
    ret

.wait_output:
    in al, 0x64
    test al, 1
    jz .wait_output
    ret

BITS 32
protected_mode:
    ; Set up segment registers
    mov ax, 0x10        ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000    ; Set up stack below 1 MiB

    ; Copy kernel from 0x10000 to 0x100000
    mov esi, 0x10000    ; Source
    mov edi, 0x100000   ; Destination
    mov ecx, KERNEL_SECTORS * 512 / 4  ; Number of DWORDs to copy
    rep movsd

    ; Jump to kernel entry point
    jmp 0x08:0x100000

; Variables
boot_drive db 0
loading_msg db 'Loading NT-like OS...', 13, 10, 0
load_ok_msg db 'Kernel loaded, entering protected mode...', 13, 10, 0
err_msg db 'Disk error! Press any key to reboot...', 0

; GDT
align 8
gdt:
    dq 0                        ; Null descriptor
    dq 0x00CF9A000000FFFF       ; 32-bit code descriptor
    dq 0x00CF92000000FFFF       ; 32-bit data descriptor
gdt_desc:
    dw $ - gdt - 1
    dd gdt

times 510-($-$$) db 0
dw 0xAA55