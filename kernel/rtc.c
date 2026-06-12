#include "rtc.h"
#include "ports.h"

static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t v)
{
    return (uint8_t)((v & 0x0F) + (v >> 4) * 10);
}

rtc_time_t rtc_read(void)
{
    rtc_time_t t;

    while (cmos_read(0x0A) & 0x80)
        ;               /* wait out an update in progress */

    t.second = cmos_read(0x00);
    t.minute = cmos_read(0x02);
    t.hour = cmos_read(0x04);

    uint8_t status_b = cmos_read(0x0B);
    if (!(status_b & 0x04)) {       /* BCD mode */
        t.second = bcd_to_bin(t.second);
        t.minute = bcd_to_bin(t.minute);
        t.hour = bcd_to_bin((uint8_t)(t.hour & 0x7F));
    }
    return t;
}
