; kernel/isr.asm
BITS 32

global isr0
global isr32
extern irq0_handler      ; C function

; common stub
%macro ISR_NOERR 1
isr%1:
    pusha
    call irq_handler
    popa
    iret
%endmacro

isr0:
    pusha
    popa
    iret

; Timer interrupt (IRQ0) -> vector 32
isr32:
    pusha
    call irq0_handler
    ; send EOI
    mov al, 0x20
    out 0x20, al
    popa
    iret