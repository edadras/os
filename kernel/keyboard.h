#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
char keyboard_getchar(void);    /* blocks (hlt) until a key arrives */
int keyboard_trygetchar(void);  /* -1 if no key pending */

#endif
