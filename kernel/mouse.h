#ifndef MOUSE_H
#define MOUSE_H

void mouse_init(int screen_w, int screen_h);
int mouse_x(void);
int mouse_y(void);
int mouse_buttons(void);    /* bit0=left, bit1=right, bit2=middle */
int mouse_take_event(void); /* 1 if state changed since last call */

#endif
