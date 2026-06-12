#include "shell.h"
#include "config.h"
#include "keyboard.h"
#include "kernel.h"
#include "ports.h"
#include "string.h"
#include "timer.h"
#include "vga.h"

#define LINE_MAX 256

static void print_logo(void)
{
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("\n");
    terminal_write("  __  __   ___   _   _  ___ \n");
    terminal_write(" |  \\/  | / _ \\ | | | ||_ _|\n");
    terminal_write(" | |\\/| || | | || |_| | | | \n");
    terminal_write(" | |  | || |_| ||  _  | | | \n");
    terminal_write(" |_|  |_| \\___/ |_| |_||___|\n");
    terminal_write("\n");
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    terminal_write("  " OS_NAME " v" OS_VERSION " - " OS_AUTHOR "\n\n");
}

static void cmd_help(void)
{
    terminal_write(
        "Available commands:\n"
        "  help            show this help\n"
        "  about           about this OS\n"
        "  logo            show the boot logo\n"
        "  clear           clear the screen\n"
        "  echo <text>     print text\n"
        "  uptime          time since boot\n"
        "  mem             installed memory (from bootloader)\n"
        "  ticks           raw timer tick count\n"
        "  color <0-15>    set text color\n"
        "  reboot          restart the machine\n"
        "  halt            stop the CPU\n");
}

static void cmd_about(void)
{
    terminal_write(OS_NAME " version " OS_VERSION "\n");
    terminal_write("A 32-bit operating system written from scratch in C and assembly.\n");
    terminal_write("Features: custom kernel, GDT/IDT, PIC, PIT timer, PS/2 keyboard\n");
    terminal_write("driver, VGA text driver, serial debug port, interactive shell.\n");
}

static void cmd_uptime(void)
{
    uint32_t secs = timer_ticks() / timer_hz();
    terminal_write("Up ");
    terminal_write_dec(secs / 3600);
    terminal_write("h ");
    terminal_write_dec((secs / 60) % 60);
    terminal_write("m ");
    terminal_write_dec(secs % 60);
    terminal_write("s\n");
}

static void cmd_mem(void)
{
    const multiboot_info_t *mb = kernel_multiboot_info();
    if (!mb || !(mb->flags & 1)) {
        terminal_write("Memory info not provided by bootloader.\n");
        return;
    }
    uint32_t total_kb = mb->mem_lower + mb->mem_upper;
    terminal_write("Lower memory: ");
    terminal_write_dec(mb->mem_lower);
    terminal_write(" KiB\nUpper memory: ");
    terminal_write_dec(mb->mem_upper);
    terminal_write(" KiB\nTotal:        ");
    terminal_write_dec(total_kb / 1024);
    terminal_write(" MiB\n");
}

static void cmd_color(const char *arg)
{
    uint32_t v = 0;
    if (*arg < '0' || *arg > '9') {
        terminal_write("Usage: color <0-15>\n");
        return;
    }
    while (*arg >= '0' && *arg <= '9')
        v = v * 10 + (uint32_t)(*arg++ - '0');
    if (v > 15) {
        terminal_write("Color must be 0-15.\n");
        return;
    }
    terminal_setcolor((uint8_t)v, VGA_BLACK);
    terminal_write("Color changed.\n");
}

static void cmd_reboot(void)
{
    terminal_write("Rebooting...\n");
    uint8_t st;
    do {
        st = inb(0x64);
    } while (st & 0x02);
    outb(0x64, 0xFE);   /* 8042 pulse reset line */
    for (;;)
        __asm__ volatile ("hlt");
}

void shell_execute(char *line)
{
    /* strip leading spaces */
    while (*line == ' ')
        line++;
    if (!*line)
        return;

    if (strcmp(line, "help") == 0)
        cmd_help();
    else if (strcmp(line, "about") == 0)
        cmd_about();
    else if (strcmp(line, "logo") == 0)
        print_logo();
    else if (strcmp(line, "clear") == 0)
        terminal_clear();
    else if (strncmp(line, "echo ", 5) == 0) {
        terminal_write(line + 5);
        terminal_putchar('\n');
    } else if (strcmp(line, "echo") == 0)
        terminal_putchar('\n');
    else if (strcmp(line, "uptime") == 0)
        cmd_uptime();
    else if (strcmp(line, "mem") == 0)
        cmd_mem();
    else if (strcmp(line, "ticks") == 0) {
        terminal_write_dec(timer_ticks());
        terminal_putchar('\n');
    } else if (strncmp(line, "color ", 6) == 0)
        cmd_color(line + 6);
    else if (strcmp(line, "reboot") == 0)
        cmd_reboot();
    else if (strcmp(line, "halt") == 0) {
        terminal_write("System halted. You can power off now.\n");
        for (;;)
            __asm__ volatile ("cli; hlt");
    } else {
        terminal_write("Unknown command: ");
        terminal_write(line);
        terminal_write("\nType 'help' for a list of commands.\n");
    }
}

void shell_banner(void)
{
    print_logo();
    terminal_write("Type 'help' to see available commands.\n\n");
}

void shell_run(void)
{
    char line[LINE_MAX];

    shell_banner();

    for (;;) {
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_write(OS_NAME "> ");
        terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);

        size_t len = 0;
        for (;;) {
            char c = keyboard_getchar();
            if (c == '\n') {
                terminal_putchar('\n');
                break;
            }
            if (c == '\b') {
                if (len > 0) {
                    len--;
                    terminal_backspace();
                }
                continue;
            }
            if (len < LINE_MAX - 1) {
                line[len++] = c;
                terminal_putchar(c);
            }
        }
        line[len] = '\0';
        shell_execute(line);
    }
}
