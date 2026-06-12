#ifndef VGA_H
#define VGA_H

#include <stdint.h>

enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15,
};

void terminal_init(void);
void terminal_clear(void);
void terminal_setcolor(uint8_t fg, uint8_t bg);
uint8_t terminal_get_fg(void);
void terminal_putchar(char c);
void terminal_write(const char *s);
void terminal_write_dec(uint32_t v);
void terminal_write_hex(uint32_t v);
void terminal_backspace(void);

/* Redirect console output (e.g. into a GUI terminal window). */
void terminal_set_hooks(void (*putchar_fn)(char), void (*clear_fn)(void));

#endif
