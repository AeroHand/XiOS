#ifndef _MOUSE_H
#define _MOUSE_H

typedef struct position {
    int32_t x;
    int32_t y;
} position_t;

typedef void (*click_handler)(int32_t, int32_t);

void init_mouse();
void hide_cursor();
void show_cursor();
int32_t add_left_click(click_handler h);
int32_t add_right_click(click_handler h);
void mouse_handler();

#endif
