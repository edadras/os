#ifndef RTC_H
#define RTC_H

#include <stdint.h>

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_time_t;

rtc_time_t rtc_read(void);

#endif
