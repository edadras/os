#include "keyboard.h"
#include "idt.h"
#include "ports.h"

#define BUF_SIZE 256

static volatile char buf[BUF_SIZE];
static volatile unsigned head;
static volatile unsigned tail;
static int shift;
static int caps;

/* US QWERTY, scancode set 1 */
static const char keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ',
};

static const char keymap_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ',
};

static void buf_push(char c)
{
    unsigned next = (head + 1) % BUF_SIZE;
    if (next != tail) {
        buf[head] = c;
        head = next;
    }
}

static void keyboard_callback(registers_t *regs)
{
    (void)regs;
    uint8_t sc = inb(0x60);

    if (sc == 0xE0)
        return;                     /* extended keys: ignore prefix */

    if (sc & 0x80) {                /* key release */
        uint8_t key = sc & 0x7F;
        if (key == 0x2A || key == 0x36)
            shift = 0;
        return;
    }

    if (sc == 0x2A || sc == 0x36) {
        shift = 1;
        return;
    }
    if (sc == 0x3A) {
        caps = !caps;
        return;
    }

    char c = shift ? keymap_shift[sc] : keymap[sc];
    if (!c)
        return;

    if (caps && !shift && c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    else if (caps && shift && c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');

    buf_push(c);
}

void keyboard_init(void)
{
    irq_register_handler(1, keyboard_callback);
}

char keyboard_getchar(void)
{
    while (head == tail)
        __asm__ volatile ("hlt");
    char c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}
