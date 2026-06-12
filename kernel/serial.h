#ifndef SERIAL_H
#define SERIAL_H

/* COM1 mirror of console output — handy for debugging under QEMU. */
void serial_init(void);
void serial_putchar(char c);
void serial_write(const char *s);

#endif
