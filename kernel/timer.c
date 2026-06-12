#include "timer.h"
#include "idt.h"
#include "ports.h"

static volatile uint32_t ticks;
static uint32_t frequency;

static void timer_callback(registers_t *regs)
{
    (void)regs;
    ticks++;
}

void timer_init(uint32_t hz)
{
    frequency = hz;
    uint32_t divisor = 1193182 / hz;

    irq_register_handler(0, timer_callback);

    outb(0x43, 0x36);   /* channel 0, lobyte/hibyte, square wave */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_ticks(void)
{
    return ticks;
}

uint32_t timer_hz(void)
{
    return frequency;
}
