; Interrupt stubs and GDT/IDT loaders.
bits 32
section .text
extern isr_dispatch

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

%assign i 0
%rep 48
  %if i == 8 || (i >= 10 && i <= 14) || i == 17 || i == 21
    ISR_ERR i
  %else
    ISR_NOERR i
  %endif
%assign i i+1
%endrep

isr_common:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    cld
    push esp
    call isr_dispatch
    add esp, 4
    pop es
    pop ds
    popa
    add esp, 8
    iretd

section .data
global isr_table
isr_table:
%assign i 0
%rep 48
    dd isr %+ i
%assign i i+1
%endrep

section .text
global gdt_load
gdt_load:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
