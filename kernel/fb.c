#include "fb.h"
#include "font8x8.h"
#include "string.h"

#define MAX_W 1280
#define MAX_H 1024

static volatile uint8_t *front;
static uint32_t width, height, pitch;
static uint32_t back[MAX_W * MAX_H];

int fb_init(const multiboot_info_t *mbi)
{
    if (!mbi || !(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER))
        return 0;
    if (mbi->framebuffer_type != 1 || mbi->framebuffer_bpp != 32)
        return 0;
    if (mbi->framebuffer_width > MAX_W || mbi->framebuffer_height > MAX_H)
        return 0;

    front = (volatile uint8_t *)(uint32_t)mbi->framebuffer_addr;
    width = mbi->framebuffer_width;
    height = mbi->framebuffer_height;
    pitch = mbi->framebuffer_pitch;
    return 1;
}

uint32_t fb_width(void)  { return width; }
uint32_t fb_height(void) { return height; }

void fb_pixel(int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= (int)width || y >= (int)height)
        return;
    back[y * width + x] = color;
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)width)  w = (int)width - x;
    if (y + h > (int)height) h = (int)height - y;
    for (int j = 0; j < h; j++) {
        uint32_t *row = &back[(y + j) * width + x];
        for (int i = 0; i < w; i++)
            row[i] = color;
    }
}

void fb_hline(int x, int y, int w, uint32_t color)
{
    fb_fill_rect(x, y, w, 1, color);
}

void fb_vline(int x, int y, int h, uint32_t color)
{
    fb_fill_rect(x, y, 1, h, color);
}

void fb_rect(int x, int y, int w, int h, uint32_t color)
{
    fb_hline(x, y, w, color);
    fb_hline(x, y + h - 1, w, color);
    fb_vline(x, y, h, color);
    fb_vline(x + w - 1, y, h, color);
}

void fb_char(int x, int y, char c, uint32_t color, int scale)
{
    if ((unsigned char)c > 127)
        c = '?';
    const unsigned char *glyph = font8x8_basic[(int)c];
    for (int row = 0; row < 8; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (!(bits & (1 << col)))
                continue;
            if (scale == 1)
                fb_pixel(x + col, y + row, color);
            else
                fb_fill_rect(x + col * scale, y + row * scale,
                             scale, scale, color);
        }
    }
}

void fb_text(int x, int y, const char *s, uint32_t color, int scale)
{
    while (*s) {
        fb_char(x, y, *s++, color, scale);
        x += 8 * scale;
    }
}

int fb_text_width(const char *s, int scale)
{
    return (int)strlen(s) * 8 * scale;
}

void fb_flip(void)
{
    for (uint32_t y = 0; y < height; y++)
        memcpy((void *)(front + y * pitch), &back[y * width], width * 4);
}
