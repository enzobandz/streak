; Fluvia entry point: Multiboot2 header (asks GRUB for a 32-bit linear
; framebuffer), stack setup, FPU enable, jump to C.
bits 32

section .multiboot2
align 8
mb2_start:
    dd 0xE85250D6                 ; magic
    dd 0                          ; architecture: i386 protected mode
    dd mb2_end - mb2_start
    dd -(0xE85250D6 + 0 + (mb2_end - mb2_start))
align 8
    dw 5, 0                       ; framebuffer tag
    dd 20
    dd 1280, 800, 32              ; preferred mode (GRUB picks the closest)
align 8
    dw 0, 0                       ; end tag
    dd 8
mb2_end:

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:

section .text
global _start
extern kmain
_start:
    cli
    mov esp, stack_top
    push ebx                      ; multiboot2 info pointer
    push eax                      ; multiboot2 magic
    ; enable the x87 FPU (used by the calculator and animation maths)
    mov ecx, cr0
    and ecx, ~(1 << 2)            ; clear EM
    or  ecx, (1 << 1) | (1 << 5)  ; set MP, NE
    mov cr0, ecx
    fninit
    call kmain
.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
