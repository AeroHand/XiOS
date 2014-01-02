// vim: tw=80:ts=4:sw=4:et:sta
#include "lib.h"
#include "mouse.h"
#include "i8259.h"
#include "task.h"

#include "colors.h"

// specifies the bits of the first packet of a movement update
#define LEFT_BUTTON (1 << 0)
#define RIGHT_BUTTON (1 << 1)
#define MIDDLE_BUTTON (1 << 2)
#define MOVEMENT_ONE (1 << 3)
#define X_SIGN (1 << 4)
#define Y_SIGN (1 << 5)
#define X_OVERFLOW (1 << 6)
#define Y_OVERFLOW (1 << 7)

#define X_SCALE 4
#define Y_SCALE 8

#define VIDEO_NUM_ROWS (NUM_ROWS+1)
#define VIDEO_NUM_COLS (NUM_COLS)

#define MOUSE_CURSOR (BACK(BRIGHT(BLUE)) | FORE(WHITE))

static position_t mouse_pos;
static coord_t prev_pos;
static uint8_t prev_attrib;
static int8_t no_notify = 0;

#define MAX_HANDLERS 3

click_handler left_click_handler[MAX_HANDLERS];
click_handler right_click_handler[MAX_HANDLERS];

enum read_status {
    ReadSuccess,
    ReadFailure,
};

enum read_status last_read;

void send_command(uint8_t command, uint8_t port);
uint8_t read_byte();
uint8_t  try_read_byte();
void write_byte(uint8_t data, uint8_t port);
void move_mouse(int32_t deltax, int32_t deltay);
void attrib_changed(int32_t x, int32_t y);

void send_command(uint8_t command, uint8_t port) {
	write_byte(0xD4, 0x64);
	write_byte(command, port);
}

uint8_t read_byte() {
	while( (inb(0x64) & 0x1) == 0 );
    last_read = ReadSuccess;
	return inb(0x60);
}

uint8_t try_read_byte() {
    if ( (inb(0x64) & 0x1) == 0 ) {
        last_read = ReadFailure;
        return 0;
    } else {
        last_read = ReadSuccess;
        return inb(0x60);
    }
}

void write_byte(uint8_t data, uint8_t port) {
	while( (inb(0x64) & 0x2) != 0 );
	outb(data, port);
}

/**
 * set the value at the mouse
 *
 * ensures that the prev_attrib is not mistakenly updated
 */
void set_mouse_attrib(int8_t attrib) {
    no_notify = 1;
    set_char_attrib(mouse_pos.x/X_SCALE, mouse_pos.y/Y_SCALE, attrib);
    no_notify = 0;
}

void hide_cursor() {
    set_mouse_attrib(prev_attrib);
}

void show_cursor() {
    set_mouse_attrib(MOUSE_CURSOR);
}

void init_mouse() {
    send_command(0xFF, 0x60);
    // send "Get Compaq Status Byte" command
	send_command(0x20, 0x64);
	uint8_t compaq_status = read_byte();
	// enable IRQ 12
	compaq_status |= 0x2;
	// clear Disable Mouse Click
	compaq_status &= ~(0x20);
    // "Send Compaq Status"
	send_command(0x60, 0x64);
	write_byte(compaq_status, 0x60);

    // these positions need to be different so that the mouse cursor is not
    // restored to prev_pos
    mouse_pos.x = 0;
    mouse_pos.y = 0;
    prev_pos.x = 0;
    prev_pos.y = 1;
    prev_attrib = get_char_attrib(prev_pos.x, prev_pos.y);
    add_attrib_observer(attrib_changed);

    int i;
    for (i = 0; i < MAX_HANDLERS; i++) {
        left_click_handler[i] = NULL;
        right_click_handler[i] = NULL;
    }

	// enable acks
	send_command(0xF4, 0x60);
}

void move_mouse(int32_t deltax, int32_t deltay) {
    prev_pos.x = mouse_pos.x/X_SCALE;
    prev_pos.y = mouse_pos.y/Y_SCALE;
    set_mouse_attrib(prev_attrib);
    // set and clamp x
    mouse_pos.x += deltax;
    if (mouse_pos.x < 0) {
        mouse_pos.x = 0;
    } else if (mouse_pos.x >= VIDEO_NUM_COLS * X_SCALE) {
        mouse_pos.x = VIDEO_NUM_COLS*X_SCALE - 1;
    }
    // set and clamp y
    mouse_pos.y -= deltay;
    if (mouse_pos.y < 0) {
        mouse_pos.y = 0;
    } else if (mouse_pos.y >= VIDEO_NUM_ROWS*Y_SCALE) {
        mouse_pos.y = VIDEO_NUM_ROWS*Y_SCALE - 1;
    }
    prev_attrib = get_char_attrib(mouse_pos.x/X_SCALE, mouse_pos.y/Y_SCALE);
    set_mouse_attrib(MOUSE_CURSOR);
}

void mouse_handler() {
	registers_t regs;
	save_regs(regs);

    uint8_t flags = try_read_byte();
    if (last_read == ReadSuccess) {
        if (flags == 0xFA) {
            // ack
        } else {
            if ( (flags & MOVEMENT_ONE) != 0 &&
                    (flags & X_OVERFLOW) == 0 &&
                    (flags & Y_OVERFLOW) == 0) {
                int32_t deltax = read_byte();
                if (flags & X_SIGN) {
                    deltax = deltax | 0xFFFFFF00;
                }
                int32_t deltay = read_byte();
                if (flags & Y_SIGN) {
                    deltay = deltay | 0xFFFFFF00;
                }
                move_mouse(deltax, deltay);
                if (flags & LEFT_BUTTON) {
                    int i;
                    for (i = 0; i < MAX_HANDLERS; i++) {
                        if (left_click_handler[i] != NULL) {
                            left_click_handler[i](mouse_pos.x/X_SCALE, mouse_pos.y/Y_SCALE);
                        }
                    }
                }
                if (flags & RIGHT_BUTTON) {
                    int i;
                    for (i = 0; i < MAX_HANDLERS; i++) {
                        if (right_click_handler[i] != NULL) {
                            right_click_handler[i](mouse_pos.x/X_SCALE, mouse_pos.y/Y_SCALE);
                        }
                    }
                }
                if (flags & MIDDLE_BUTTON) {
                }
            }
        }
    }

	send_eoi(12);

	restore_regs(regs);
	asm ("leave; \
		 	iret; \
			");
}

void attrib_changed(int32_t x, int32_t y) {
    if (no_notify) {
        return;
    }
    if (x == prev_pos.x && y == prev_pos.y) {
        prev_attrib = get_char_attrib(x, y);
    }
}

/**
 * add a left click handler
 * 
 * returns 0 if successful, -1 if no more handlers available
 */
int32_t add_left_click(click_handler h) {
    int i = 0;
    for (i = 0; i < MAX_HANDLERS; i++) {
        if (left_click_handler[i] == NULL) {
            left_click_handler[i] = h;
            return 0;
        }
    }
    return -1;
}

/**
 * add a right click handler
 * 
 * returns 0 if successful, -1 if no more handlers available
 */
int32_t add_right_click(click_handler h) {
    int i = 0;
    for (i = 0; i < MAX_HANDLERS; i++) {
        if (right_click_handler[i] == NULL) {
            right_click_handler[i] = h;
            return 0;
        }
    }
    return -1;
}
