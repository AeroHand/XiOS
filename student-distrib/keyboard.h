/*
 * keyboard.h
 *
 * functions for the keyboard driver
 */

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "lib.h"
#include "fs.h"
#include "task.h"

#define NUM_TERMINALS 10
#define BUFFER_SIZE (((NUM_ROWS) * NUM_COLS) + 1 - 7)// +1 is for terminating '\0', -7 is for prompt size

#define NO_TERMINAL -1
#define VIDEO_ADDRESS(tid) (0x1000 * (tid+1))
#define MAX_HISTORY_CMDS 16

/**
 * Struct that defines a "terminal object".
  */
typedef struct terminal_info {
    // The ID number of the terminal (referred to as tid elsewhere).
	uint32_t index;

	// A flag that determines whether the terminal has data to be read.
    uint8_t keyboard_read_flag;

	// Coordinates to keep track of the status of the keyboard buffer.
    coord_t keyboard_start_coord;
    uint32_t keyboard_buffer_pos;
    uint32_t keyboard_buffer_size;

	// The keyboard buffer itself.
    int8_t keyboard_buffer[BUFFER_SIZE];
	
	// The buffer used to store the values to be returned in a keyboard read.
    int8_t keyboard_read_buffer[BUFFER_SIZE];

	// Variables used for command history.
	uint32_t history_size;
	int32_t history_curr;
	int8_t command_history[MAX_HISTORY_CMDS][BUFFER_SIZE];

	// The position of the cursor on the terminal screen when it is
	// remapped to a backing page.  This position is restored when the
	// terminal becomes visible again.
    coord_t current_position;

	// The pointer to the current terminal's video memory.
	// This will be either real video memory (0xB8000)
	// or a backing page.
    char* video_memory;
    char* video_memory_base;
    
} terminal_info_t;

// Information about the system's terminals.
extern terminal_info_t* terminals;

// Information about the system's terminals.
extern process_t* process_in_terminal[NUM_TERMINALS];

// Pointer to the current (visible) terminal.
extern terminal_info_t* current_terminal;

// Interrupt handler for a keypress.
void keypress_handler(void);

// Functions to handle special keys.
void handle_backspace(void);
void handle_enter(void);
void handle_tab(void);

// History functions.
void history_move(int32_t offset);

// Tab complete functions.
void tab_complete(void);

// Keyboard-specific syscalls.
int32_t keyboard_read(file_info_t *file, uint8_t* buf, int32_t nbytes);
int32_t keyboard_write(file_info_t *file, const int8_t* buf, int32_t nbytes);
int32_t keyboard_open(void);
int32_t keyboard_close(file_info_t *file);

// Initialization functions.
int32_t init_terminals(void);
void reset_keyboard(void);

// Function to print the keyboard buffer to the screen.
void reprint_keyboard_buffer();

// Left click on current line handler
void line_click(int x, int y);

// Terminal video backing page manipulation functions.
void clear_terminal_backing_page(terminal_info_t *terminal);
void scroll_backing(terminal_info_t *terminal);
void putc_to_backing(uint8_t c, terminal_info_t *terminal);

// Terminal manipulation functions.
terminal_info_t *new_terminal(void);
void switch_terminals(terminal_info_t *terminal);
void unmap_backing_page(terminal_info_t *terminal);
void map_backing_page(terminal_info_t *terminal);

// Scrollback buffer manipulation functions.
void adjust_scrollback_page(int32_t offset);
void set_scrollback_page(int32_t value);
void load_scrollback_page(int32_t offset);
void map_base_page(terminal_info_t *terminal);
void unmap_base_page(terminal_info_t *terminal);

#endif /* _KEYBOARD_H */
