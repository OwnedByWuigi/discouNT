section .text

global exception_handler
global isr_stub_0, isr_stub_1, isr_stub_2, isr_stub_3
global isr_stub_4, isr_stub_5, isr_stub_6, isr_stub_7
global isr_stub_8, isr_stub_9, isr_stub_10, isr_stub_11
global isr_stub_12, isr_stub_13, isr_stub_14

extern exception_handler_c

; Common exception handler
exception_handler:
    pusha
    push dword [esp+36]  ; EIP
    push dword [esp+36]  ; Error code  
    push dword [esp+36]  ; Exception number (pushed by stub)
    call exception_handler_c
    add esp, 12
    popa
    add esp, 4           ; Remove error code
    iret

; Stubs for exceptions with error code
%macro STUB_ERR 1
isr_stub_%1:
    push dword %1
    jmp exception_handler
%endmacro

; Stubs for exceptions without error code
%macro STUB_NOERR 1
isr_stub_%1:
    push dword 0         ; Dummy error code
    push dword %1
    jmp exception_handler
%endmacro

STUB_NOERR 0
STUB_NOERR 1
STUB_NOERR 2
STUB_NOERR 3
STUB_NOERR 4
STUB_NOERR 5
STUB_NOERR 6
STUB_NOERR 7
STUB_ERR   8
STUB_NOERR 9
STUB_ERR   10
STUB_ERR   11
STUB_ERR   12
STUB_ERR   13
STUB_ERR   14