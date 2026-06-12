#include "vga.h"
#include "ports.h"
#include "string.h"
#include "serial.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEM    ((volatile uint16_t *)0xB8000)

static size_t row;
static size_t col;
static uint8_t color;

static uint16_t vga_entry(char c, uint8_t clr)
{
    return (uint16_t)c | ((uint16_t)clr << 8);
}

static void update_cursor(void)
{
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void terminal_setcolor(uint8_t fg, uint8_t bg)
{
    color = (uint8_t)(fg | (bg << 4));
}

void terminal_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = vga_entry(' ', color);
    row = 0;
    col = 0;
    update_cursor();
}

void terminal_init(void)
{
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    terminal_clear();
}

static void scroll(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[(y - 1) * VGA_WIDTH + x] = VGA_MEM[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c)
{
    serial_putchar(c);

    if (c == '\n') {
        col = 0;
        row++;
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\t') {
        col = (col + 8) & ~(size_t)7;
    } else {
        VGA_MEM[row * VGA_WIDTH + col] = vga_entry(c, color);
        col++;
    }

    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
    }
    if (row >= VGA_HEIGHT)
        scroll();
    update_cursor();
}

void terminal_backspace(void)
{
    if (col == 0 && row == 0)
        return;
    if (col == 0) {
        row--;
        col = VGA_WIDTH - 1;
    } else {
        col--;
    }
    VGA_MEM[row * VGA_WIDTH + col] = vga_entry(' ', color);
    update_cursor();
    serial_write("\b \b");
}

void terminal_write(const char *s)
{
    while (*s)
        terminal_putchar(*s++);
}

void terminal_write_dec(uint32_t v)
{
    char buf[12];
    utoa(v, buf, 10);
    terminal_write(buf);
}

void terminal_write_hex(uint32_t v)
{
    char buf[12];
    utoa(v, buf, 16);
    terminal_write("0x");
    terminal_write(buf);
}
