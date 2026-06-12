#include "mouse.h"
#include "idt.h"
#include "ports.h"

static volatile int x, y, buttons;
static volatile int changed;
static int max_x, max_y;
static uint8_t packet[3];
static int cycle;

static void wait_write(void)
{
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 0x02))
            return;
}

static void wait_read(void)
{
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 0x01)
            return;
}

static void mouse_send(uint8_t b)
{
    wait_write();
    outb(0x64, 0xD4);
    wait_write();
    outb(0x60, b);
    wait_read();
    inb(0x60);          /* consume ACK */
}

static void mouse_callback(registers_t *regs)
{
    (void)regs;
    uint8_t b = inb(0x60);

    if (cycle == 0 && !(b & 0x08))
        return;         /* out of sync: wait for a header byte */

    packet[cycle++] = b;
    if (cycle < 3)
        return;
    cycle = 0;

    int dx = (int)packet[1] - ((packet[0] & 0x10) ? 256 : 0);
    int dy = (int)packet[2] - ((packet[0] & 0x20) ? 256 : 0);

    x += dx;
    y -= dy;            /* PS/2 y axis is inverted vs. screen */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > max_x) x = max_x;
    if (y > max_y) y = max_y;
    buttons = packet[0] & 0x07;
    changed = 1;
}

void mouse_init(int screen_w, int screen_h)
{
    max_x = screen_w - 1;
    max_y = screen_h - 1;
    x = screen_w / 2;
    y = screen_h / 2;

    wait_write();
    outb(0x64, 0xA8);   /* enable auxiliary device */

    wait_write();
    outb(0x64, 0x20);   /* read controller command byte */
    wait_read();
    uint8_t status = inb(0x60);
    status |= 0x02;     /* enable IRQ12 */
    status &= (uint8_t)~0x20;   /* enable mouse clock */
    wait_write();
    outb(0x64, 0x60);
    wait_write();
    outb(0x60, status);

    mouse_send(0xF6);   /* set defaults */
    mouse_send(0xF4);   /* enable data reporting */

    irq_register_handler(12, mouse_callback);
}

int mouse_x(void)       { return x; }
int mouse_y(void)       { return y; }
int mouse_buttons(void) { return buttons; }

int mouse_take_event(void)
{
    if (!changed)
        return 0;
    changed = 0;
    return 1;
}
