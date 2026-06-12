; AriaOS bootstrap — Multiboot entry point
; GRUB loads this, sets EAX=magic, EBX=multiboot info pointer.

BITS 32

MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x00000003          ; page-align modules + provide memory map
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .text
global _start
extern kmain

_start:
    mov esp, stack_top
    push ebx                        ; multiboot info pointer
    push eax                        ; multiboot magic
    call kmain

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384                      ; 16 KiB kernel stack
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
