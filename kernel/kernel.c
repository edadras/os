#include "kernel.h"
#include "config.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "serial.h"
#include "shell.h"
#include "timer.h"
#include "vga.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

static const multiboot_info_t *mb_info;

const multiboot_info_t *kernel_multiboot_info(void)
{
    return mb_info;
}

void kmain(uint32_t magic, const multiboot_info_t *mbi)
{
    if (magic == MULTIBOOT_BOOTLOADER_MAGIC)
        mb_info = mbi;

    serial_init();
    terminal_init();

    terminal_write("[boot] " OS_NAME " " OS_VERSION " starting...\n");

    gdt_init();
    terminal_write("[boot] GDT loaded\n");

    idt_init();
    terminal_write("[boot] IDT + PIC ready\n");

    timer_init(100);
    terminal_write("[boot] PIT timer at 100 Hz\n");

    keyboard_init();
    terminal_write("[boot] PS/2 keyboard driver ready\n");

    __asm__ volatile ("sti");
    terminal_write("[boot] interrupts enabled\n");

    shell_run();
}
