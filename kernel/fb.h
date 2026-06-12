#ifndef FB_H
#define FB_H

#include <stdint.h>
#include "kernel.h"

/* Linear framebuffer driver (32 bpp RGB). All drawing goes to a back
 * buffer; fb_flip() pushes it to the screen. */

int fb_init(const multiboot_info_t *mbi);   /* 1 on success */
uint32_t fb_width(void);
uint32_t fb_height(void);

void fb_pixel(int x, int y, uint32_t color);
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void fb_rect(int x, int y, int w, int h, uint32_t color);
void fb_hline(int x, int y, int w, uint32_t color);
void fb_vline(int x, int y, int h, uint32_t color);
void fb_char(int x, int y, char c, uint32_t color, int scale);
void fb_text(int x, int y, const char *s, uint32_t color, int scale);
int  fb_text_width(const char *s, int scale);
void fb_flip(void);

#endif
