BITS 64
section .text
global exception_handler
%assign i 0
%rep 15
global isr_stub_%+i
isr_stub_%+i:
    push 0
    push i
    jmp exception_handler
%assign i i+1
%endrep
exception_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq
