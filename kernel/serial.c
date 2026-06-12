#include "serial.h"
#include "ports.h"

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);   /* disable interrupts */
    outb(COM1 + 3, 0x80);   /* enable DLAB */
    outb(COM1 + 0, 0x03);   /* 38400 baud divisor low */
    outb(COM1 + 1, 0x00);   /* divisor high */
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0xC7);   /* FIFO enabled, cleared, 14-byte threshold */
    outb(COM1 + 4, 0x0B);   /* RTS/DSR set */
}

void serial_putchar(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
        ;
    if (c == '\n')
        serial_putchar('\r');
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s)
        serial_putchar(*s++);
}
