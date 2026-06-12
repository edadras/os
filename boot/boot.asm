; AriaOS bootstrap — Multiboot entry point
; GRUB loads this, sets EAX=magic, EBX=multiboot info pointer.

BITS 32

MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x00000007          ; page-align + memory map + video mode
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
    dd 0, 0, 0, 0, 0                ; load addresses (unused, bit 16 clear)
    dd 0                            ; mode type: linear graphics
    dd 1024                         ; width
    dd 768                          ; height
    dd 32                           ; bits per pixel

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
