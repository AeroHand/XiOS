#ifndef _COLORS_H
#define _COLORS_H

#define VIDEO 0xB8000
#define FORE(color) color
#define BACK(color) (color << 4)
#define BRIGHT(color) (color | 0x8)
#define BLACK 0x0
#define BLUE 0x1
#define GREEN 0x2
#define CYAN 0x3
#define RED 0x4
#define MAGENTA 0x5
#define BROWN 0x6
#define GRAY 0x7
#define WHITE BRIGHT(GRAY)

#define ATTRIB FORE(WHITE) | BACK(BLACK)
#define CURSOR_ATTRIB FORE(WHITE) | BACK(GRAY)


#endif
