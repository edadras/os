#include "gui.h"
#include "config.h"
#include "fb.h"
#include "kernel.h"
#include "keyboard.h"
#include "mouse.h"
#include "rtc.h"
#include "shell.h"
#include "string.h"
#include "timer.h"
#include "vga.h"

/* ------------------------------------------------------------------ */
/* Theme (classic Windows look)                                        */
/* ------------------------------------------------------------------ */

#define COL_DESKTOP   0x008080
#define COL_FACE      0xC0C0C0
#define COL_LIGHT     0xFFFFFF
#define COL_SHADOW    0x808080
#define COL_DARK      0x000000
#define COL_TITLE     0x000080
#define COL_TITLE_TXT 0xFFFFFF
#define COL_MENU_HI   0x000080

#define TASKBAR_H   34
#define TITLE_H     22
#define CLOSE_W     18

/* VGA 16-color palette → RGB, so shell color codes work in the GUI */
static const uint32_t vga_rgb[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

/* ------------------------------------------------------------------ */
/* Windows                                                             */
/* ------------------------------------------------------------------ */

typedef enum { WIN_TERMINAL, WIN_ABOUT, WIN_SYSINFO } win_type_t;

typedef struct {
    int used;
    int x, y, w, h;
    win_type_t type;
    const char *title;
} window_t;

#define MAX_WIN 4
static window_t wins[MAX_WIN];
static int zorder[MAX_WIN];     /* indices into wins[], back to front */
static int nwin;

static int start_open;
static int drag_win = -1;
static int drag_dx, drag_dy;
static int prev_buttons;
static int dirty = 1;

/* ------------------------------------------------------------------ */
/* Terminal window state                                               */
/* ------------------------------------------------------------------ */

#define TERM_COLS 64
#define TERM_ROWS 18
#define TERM_LINE_H 11

static char tchar[TERM_ROWS][TERM_COLS];
static uint8_t tcolor[TERM_ROWS][TERM_COLS];
static int tcx, tcy;
static char tline[128];
static int tlen;

static void term_clear(void)
{
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++) {
            tchar[r][c] = ' ';
            tcolor[r][c] = VGA_LIGHT_GREY;
        }
    tcx = 0;
    tcy = 0;
}

static void term_scroll(void)
{
    for (int r = 1; r < TERM_ROWS; r++) {
        memcpy(tchar[r - 1], tchar[r], TERM_COLS);
        memcpy(tcolor[r - 1], tcolor[r], TERM_COLS);
    }
    for (int c = 0; c < TERM_COLS; c++) {
        tchar[TERM_ROWS - 1][c] = ' ';
        tcolor[TERM_ROWS - 1][c] = VGA_LIGHT_GREY;
    }
    tcy = TERM_ROWS - 1;
}

static void term_putchar(char c)
{
    if (c == '\n') {
        tcx = 0;
        tcy++;
    } else if (c == '\r') {
        tcx = 0;
    } else if (c == '\b') {
        if (tcx > 0) {
            tcx--;
            tchar[tcy][tcx] = ' ';
        }
    } else if (c == '\t') {
        tcx = (tcx + 8) & ~7;
    } else {
        tchar[tcy][tcx] = c;
        tcolor[tcy][tcx] = terminal_get_fg();
        tcx++;
    }
    if (tcx >= TERM_COLS) {
        tcx = 0;
        tcy++;
    }
    if (tcy >= TERM_ROWS)
        term_scroll();
    dirty = 1;
}

static void term_prompt(void)
{
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write(OS_NAME "> ");
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
}

/* ------------------------------------------------------------------ */
/* Window management                                                   */
/* ------------------------------------------------------------------ */

static void win_focus(int idx)
{
    int pos = -1;
    for (int i = 0; i < nwin; i++)
        if (zorder[i] == idx)
            pos = i;
    if (pos < 0)
        return;
    for (int i = pos; i < nwin - 1; i++)
        zorder[i] = zorder[i + 1];
    zorder[nwin - 1] = idx;
}

static int win_focused(void)
{
    return nwin > 0 ? zorder[nwin - 1] : -1;
}

static void win_close(int idx)
{
    wins[idx].used = 0;
    int pos = -1;
    for (int i = 0; i < nwin; i++)
        if (zorder[i] == idx)
            pos = i;
    if (pos < 0)
        return;
    for (int i = pos; i < nwin - 1; i++)
        zorder[i] = zorder[i + 1];
    nwin--;
}

static void win_open(win_type_t type)
{
    for (int i = 0; i < MAX_WIN; i++) {
        if (wins[i].used && wins[i].type == type) {
            win_focus(i);
            return;
        }
    }
    int slot = -1;
    for (int i = 0; i < MAX_WIN; i++)
        if (!wins[i].used)
            slot = i;
    if (slot < 0 || nwin >= MAX_WIN)
        return;

    window_t *w = &wins[slot];
    w->used = 1;
    w->type = type;
    switch (type) {
    case WIN_TERMINAL:
        w->title = "Terminal";
        w->w = TERM_COLS * 8 + 16;
        w->h = TITLE_H + TERM_ROWS * TERM_LINE_H + 16;
        w->x = 60;
        w->y = 60;
        break;
    case WIN_ABOUT:
        w->title = "About " OS_NAME;
        w->w = 420;
        w->h = 200;
        w->x = 280;
        w->y = 180;
        break;
    case WIN_SYSINFO:
        w->title = "System Info";
        w->w = 340;
        w->h = 190;
        w->x = 330;
        w->y = 220;
        break;
    }
    zorder[nwin++] = slot;
}

/* ------------------------------------------------------------------ */
/* Drawing helpers                                                     */
/* ------------------------------------------------------------------ */

static void bevel(int x, int y, int w, int h, int raised)
{
    uint32_t tl = raised ? COL_LIGHT : COL_SHADOW;
    uint32_t br = raised ? COL_SHADOW : COL_LIGHT;
    fb_hline(x, y, w, tl);
    fb_vline(x, y, h, tl);
    fb_hline(x, y + h - 1, w, br);
    fb_vline(x + w - 1, y, h, br);
}

static void draw_button(int x, int y, int w, int h, const char *label,
                        int pressed)
{
    fb_fill_rect(x, y, w, h, COL_FACE);
    bevel(x, y, w, h, !pressed);
    int tx = x + (w - fb_text_width(label, 1)) / 2;
    int ty = y + (h - 8) / 2;
    fb_text(tx + (pressed ? 1 : 0), ty + (pressed ? 1 : 0), label, COL_DARK, 1);
}

static const char *cursor_img[12] = {
    "X           ",
    "XX          ",
    "XoX         ",
    "XooX        ",
    "XoooX       ",
    "XooooX      ",
    "XoooooX     ",
    "XooooooX    ",
    "XoooXXXX    ",
    "XoX XoX     ",
    "X    XoX    ",
    "     XX     ",
};

static void draw_cursor(int mx, int my)
{
    for (int r = 0; r < 12; r++)
        for (int c = 0; c < 12; c++) {
            char p = cursor_img[r][c];
            if (p == 'X')
                fb_pixel(mx + c, my + r, 0x000000);
            else if (p == 'o')
                fb_pixel(mx + c, my + r, 0xFFFFFF);
        }
}

static void draw_window_frame(const window_t *w, int focused)
{
    fb_fill_rect(w->x, w->y, w->w, w->h, COL_FACE);
    bevel(w->x, w->y, w->w, w->h, 1);
    fb_fill_rect(w->x + 3, w->y + 3, w->w - 6, TITLE_H - 3,
                 focused ? COL_TITLE : COL_SHADOW);
    fb_text(w->x + 8, w->y + 3 + (TITLE_H - 3 - 8) / 2, w->title,
            COL_TITLE_TXT, 1);
    /* close button */
    int cx = w->x + w->w - 3 - CLOSE_W;
    int cy = w->y + 5;
    fb_fill_rect(cx, cy, CLOSE_W, TITLE_H - 7, COL_FACE);
    bevel(cx, cy, CLOSE_W, TITLE_H - 7, 1);
    fb_text(cx + (CLOSE_W - 8) / 2, cy + (TITLE_H - 7 - 8) / 2, "x",
            COL_DARK, 1);
}

static void draw_terminal(const window_t *w)
{
    int ix = w->x + 8;
    int iy = w->y + TITLE_H + 6;
    fb_fill_rect(ix - 2, iy - 2, TERM_COLS * 8 + 4,
                 TERM_ROWS * TERM_LINE_H + 4, 0x000000);
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++) {
            char ch = tchar[r][c];
            if (ch != ' ')
                fb_char(ix + c * 8, iy + r * TERM_LINE_H, ch,
                        vga_rgb[tcolor[r][c]], 1);
        }
    /* blinking-style cursor block */
    fb_fill_rect(ix + tcx * 8, iy + tcy * TERM_LINE_H + 8, 8, 2, 0xAAAAAA);
}

static void draw_about(const window_t *w)
{
    int x = w->x + 16;
    int y = w->y + TITLE_H + 14;
    fb_text(x, y, OS_NAME, COL_TITLE, 3);
    y += 34;
    fb_text(x, y, "Version " OS_VERSION, COL_DARK, 1);
    y += 16;
    fb_text(x, y, "A 32-bit operating system built from scratch", COL_DARK, 1);
    y += 12;
    fb_text(x, y, "in C and assembly. Custom kernel, drivers,", COL_DARK, 1);
    y += 12;
    fb_text(x, y, "window manager - no Linux inside.", COL_DARK, 1);
    y += 24;
    fb_text(x, y, "Every pixel on this screen is your code.", COL_SHADOW, 1);
}

static void draw_sysinfo(const window_t *w)
{
    char buf[16];
    int x = w->x + 16;
    int y = w->y + TITLE_H + 14;

    fb_text(x, y, "Resolution:", COL_DARK, 1);
    utoa(fb_width(), buf, 10);
    fb_text(x + 110, y, buf, COL_DARK, 1);
    fb_text(x + 110 + fb_text_width(buf, 1), y, " x ", COL_DARK, 1);
    utoa(fb_height(), buf, 10);
    fb_text(x + 110 + fb_text_width(buf, 1) + 24, y, buf, COL_DARK, 1);

    y += 18;
    fb_text(x, y, "Memory:", COL_DARK, 1);
    const multiboot_info_t *mb = kernel_multiboot_info();
    if (mb && (mb->flags & MULTIBOOT_INFO_MEMORY)) {
        utoa((mb->mem_lower + mb->mem_upper) / 1024, buf, 10);
        fb_text(x + 110, y, buf, COL_DARK, 1);
        fb_text(x + 110 + fb_text_width(buf, 1), y, " MiB", COL_DARK, 1);
    } else {
        fb_text(x + 110, y, "unknown", COL_DARK, 1);
    }

    y += 18;
    fb_text(x, y, "Uptime:", COL_DARK, 1);
    uint32_t secs = timer_ticks() / timer_hz();
    utoa(secs / 60, buf, 10);
    fb_text(x + 110, y, buf, COL_DARK, 1);
    fb_text(x + 110 + fb_text_width(buf, 1), y, " min ", COL_DARK, 1);
    utoa(secs % 60, buf, 10);
    fb_text(x + 110 + fb_text_width(buf, 1) + 40, y, buf, COL_DARK, 1);
    fb_text(x + 110 + fb_text_width(buf, 1) + 40 + fb_text_width(buf, 1),
            y, " sec", COL_DARK, 1);

    y += 18;
    fb_text(x, y, "Kernel:", COL_DARK, 1);
    fb_text(x + 110, y, OS_NAME " " OS_VERSION " (32-bit)", COL_DARK, 1);

    y += 18;
    fb_text(x, y, "Timer:", COL_DARK, 1);
    utoa(timer_hz(), buf, 10);
    fb_text(x + 110, y, buf, COL_DARK, 1);
    fb_text(x + 110 + fb_text_width(buf, 1), y, " Hz (PIT)", COL_DARK, 1);
}

/* ------------------------------------------------------------------ */
/* Start menu                                                          */
/* ------------------------------------------------------------------ */

#define MENU_ITEMS 4
#define MENU_W     180
#define MENU_ITEM_H 26

static const char *menu_labels[MENU_ITEMS] = {
    "Terminal", "About " OS_NAME, "System Info", "Reboot",
};

static int menu_x(void) { return 4; }
static int menu_y(void)
{
    return (int)fb_height() - TASKBAR_H - MENU_ITEMS * MENU_ITEM_H - 8;
}

static void draw_start_menu(int mx, int my)
{
    int x = menu_x();
    int y = menu_y();
    int h = MENU_ITEMS * MENU_ITEM_H + 8;
    fb_fill_rect(x, y, MENU_W, h, COL_FACE);
    bevel(x, y, MENU_W, h, 1);
    for (int i = 0; i < MENU_ITEMS; i++) {
        int iy = y + 4 + i * MENU_ITEM_H;
        int hover = mx >= x + 4 && mx < x + MENU_W - 4 &&
                    my >= iy && my < iy + MENU_ITEM_H;
        if (hover)
            fb_fill_rect(x + 4, iy, MENU_W - 8, MENU_ITEM_H, COL_MENU_HI);
        fb_text(x + 14, iy + (MENU_ITEM_H - 8) / 2, menu_labels[i],
                hover ? COL_LIGHT : COL_DARK, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Taskbar                                                             */
/* ------------------------------------------------------------------ */

static void draw_taskbar(void)
{
    int W = (int)fb_width();
    int H = (int)fb_height();
    int y = H - TASKBAR_H;

    fb_fill_rect(0, y, W, TASKBAR_H, COL_FACE);
    fb_hline(0, y, W, COL_LIGHT);

    draw_button(4, y + 4, 64, TASKBAR_H - 8, OS_NAME, start_open);

    /* one taskbar button per open window */
    int bx = 76;
    for (int i = 0; i < nwin; i++) {
        const window_t *w = &wins[zorder[i]];
        draw_button(bx, y + 4, 120, TASKBAR_H - 8, w->title,
                    zorder[i] == win_focused());
        bx += 126;
    }

    /* clock */
    rtc_time_t t = rtc_read();
    char clk[9];
    clk[0] = (char)('0' + t.hour / 10);
    clk[1] = (char)('0' + t.hour % 10);
    clk[2] = ':';
    clk[3] = (char)('0' + t.minute / 10);
    clk[4] = (char)('0' + t.minute % 10);
    clk[5] = ':';
    clk[6] = (char)('0' + t.second / 10);
    clk[7] = (char)('0' + t.second % 10);
    clk[8] = '\0';
    int cw = fb_text_width(clk, 1) + 16;
    fb_fill_rect(W - cw - 4, y + 4, cw, TASKBAR_H - 8, COL_FACE);
    bevel(W - cw - 4, y + 4, cw, TASKBAR_H - 8, 0);
    fb_text(W - cw + 4, y + 4 + (TASKBAR_H - 8 - 8) / 2, clk, COL_DARK, 1);
}

/* ------------------------------------------------------------------ */
/* Scene                                                               */
/* ------------------------------------------------------------------ */

static void draw_all(void)
{
    int W = (int)fb_width();
    int H = (int)fb_height();

    fb_fill_rect(0, 0, W, H, COL_DESKTOP);
    fb_text((W - fb_text_width(OS_NAME, 4)) / 2, H / 2 - 60,
            OS_NAME, 0x00A0A0, 4);

    for (int i = 0; i < nwin; i++) {
        const window_t *w = &wins[zorder[i]];
        draw_window_frame(w, zorder[i] == win_focused());
        switch (w->type) {
        case WIN_TERMINAL: draw_terminal(w); break;
        case WIN_ABOUT:    draw_about(w);    break;
        case WIN_SYSINFO:  draw_sysinfo(w);  break;
        }
    }

    draw_taskbar();
    if (start_open)
        draw_start_menu(mouse_x(), mouse_y());
    draw_cursor(mouse_x(), mouse_y());
    fb_flip();
}

/* ------------------------------------------------------------------ */
/* Input handling                                                      */
/* ------------------------------------------------------------------ */

static void menu_action(int item)
{
    switch (item) {
    case 0: win_open(WIN_TERMINAL); break;
    case 1: win_open(WIN_ABOUT);    break;
    case 2: win_open(WIN_SYSINFO);  break;
    case 3: {
        char cmd[] = "reboot";
        shell_execute(cmd);
        break;
    }
    }
}

static void handle_click(int mx, int my)
{
    int H = (int)fb_height();

    /* start button */
    if (my >= H - TASKBAR_H + 4 && my < H - 4 && mx >= 4 && mx < 68) {
        start_open = !start_open;
        return;
    }

    /* start menu items */
    if (start_open) {
        int x = menu_x(), y = menu_y();
        if (mx >= x && mx < x + MENU_W &&
            my >= y && my < y + MENU_ITEMS * MENU_ITEM_H + 8) {
            int item = (my - y - 4) / MENU_ITEM_H;
            if (item >= 0 && item < MENU_ITEMS)
                menu_action(item);
            start_open = 0;
            return;
        }
        start_open = 0;
    }

    /* taskbar window buttons */
    if (my >= H - TASKBAR_H) {
        int bx = 76;
        for (int i = 0; i < nwin; i++) {
            if (mx >= bx && mx < bx + 120) {
                win_focus(zorder[i]);
                return;
            }
            bx += 126;
        }
        return;
    }

    /* windows, topmost first */
    for (int i = nwin - 1; i >= 0; i--) {
        int idx = zorder[i];
        window_t *w = &wins[idx];
        if (mx < w->x || mx >= w->x + w->w || my < w->y || my >= w->y + w->h)
            continue;
        /* close button */
        int cx = w->x + w->w - 3 - CLOSE_W;
        if (mx >= cx && mx < cx + CLOSE_W &&
            my >= w->y + 5 && my < w->y + TITLE_H - 2) {
            win_close(idx);
            return;
        }
        win_focus(idx);
        if (my < w->y + TITLE_H) {      /* drag from title bar */
            drag_win = idx;
            drag_dx = mx - w->x;
            drag_dy = my - w->y;
        }
        return;
    }
}

static void handle_mouse(void)
{
    int mx = mouse_x();
    int my = mouse_y();
    int btn = mouse_buttons();

    if ((btn & 1) && !(prev_buttons & 1))
        handle_click(mx, my);

    if (!(btn & 1))
        drag_win = -1;

    if (drag_win >= 0) {
        wins[drag_win].x = mx - drag_dx;
        wins[drag_win].y = my - drag_dy;
        if (wins[drag_win].y < 0)
            wins[drag_win].y = 0;
    }

    prev_buttons = btn;
    dirty = 1;
}

static void handle_key(char c)
{
    int f = win_focused();
    if (f < 0 || wins[f].type != WIN_TERMINAL)
        return;

    if (c == '\n') {
        term_putchar('\n');
        tline[tlen] = '\0';
        tlen = 0;
        shell_execute(tline);
        term_prompt();
    } else if (c == '\b') {
        if (tlen > 0) {
            tlen--;
            term_putchar('\b');
        }
    } else if (tlen < (int)sizeof(tline) - 1) {
        tline[tlen++] = c;
        term_putchar(c);
    }
}

/* ------------------------------------------------------------------ */

void gui_run(void)
{
    mouse_init((int)fb_width(), (int)fb_height());

    term_clear();
    terminal_set_hooks(term_putchar, term_clear);

    win_open(WIN_TERMINAL);
    shell_banner();
    term_prompt();

    uint8_t last_sec = 0xFF;

    for (;;) {
        int c;
        while ((c = keyboard_trygetchar()) >= 0) {
            handle_key((char)c);
            dirty = 1;
        }
        if (mouse_take_event())
            handle_mouse();

        rtc_time_t t = rtc_read();
        if (t.second != last_sec) {
            last_sec = t.second;
            dirty = 1;
        }

        if (dirty) {
            dirty = 0;
            draw_all();
        }
        __asm__ volatile ("hlt");
    }
}
