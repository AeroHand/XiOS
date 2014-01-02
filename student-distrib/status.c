#include "lib.h"
#include "status.h"
#include "keyboard.h"
#include "colors.h"
#include "mouse.h"
#include "syscall.h"
#include "spinlock.h"

status_bar_t status;
int32_t first_visible_segment = 0;

void init_status(void) {
	int i;
	for (i = 0; i < NUM_COLS; i++) {
		status.segments[i].attrib = INACTIVE_STATUS_ATTRIB;
	}
	if (add_left_click(status_click) != 0) {
		// handler not actually set
	}
}

// Prints the first 80 character of the given segments to the screen.
int32_t write_status_bar(void)
{
	int8_t current_attrib = 0x04;
	int8_t write_delimiter_flag = 0;
	int32_t num_chars_written = 0;
	int32_t current_segment = 0;
	int32_t position_in_segment = 0;
	uint32_t x = 0;

	// We can't address at 0 for now.  So, leave it blank.
	//num_chars_written++;
	//x++;

	write_status_char('|', INACTIVE_STATUS_ATTRIB, 0);
	num_chars_written++;
	x++;

	while(num_chars_written < NUM_COLS)
	{
		// Check to see if we're done with a segment.  If so, move to the next one.
		while((current_segment < NUM_COLS) && 
			((status.segments[current_segment].data[position_in_segment] == '\0') || 
				(status.segments[current_segment].length <= position_in_segment)))
		{
			current_segment++;
			position_in_segment = 0;
			write_delimiter_flag = 1;
		}

		if(write_delimiter_flag)
		{
			write_status_char('|', INACTIVE_STATUS_ATTRIB, x);
			num_chars_written++;
			x++;
			write_delimiter_flag = 0;
			continue;
		}

		// If we've run out of data to print, just print a pipe and then spaces.
		if(current_segment == NUM_COLS)
		{
			while(num_chars_written < NUM_COLS)
			{
				write_status_char(' ', current_attrib, x);
				num_chars_written++;
				x++;
			}
		}

		// If we actually have data to print, print the character.
		write_status_char(status.segments[current_segment].data[position_in_segment], status.segments[current_segment].attrib, x);
		num_chars_written++;
		position_in_segment++;
		x++;
	}

	return 0;
}

void write_status_char(int8_t theChar, int8_t current_attrib, uint32_t x)
{
	x %= (NUM_COLS+1);
	*(uint8_t *)(VIDEO + ((NUM_COLS* (NUM_ROWS) + x) << 1)) = theChar;
	set_char_attrib(x, NUM_ROWS, current_attrib);
}

void set_segment_data(int32_t segment_number, int8_t* data)
{
	int8_t segment_buffer[20] = "";
	strlcat(segment_buffer, data, 20);
	status.segments[segment_number].length = strlen(segment_buffer);
	strncpy((int8_t*)status.segments[segment_number].data, segment_buffer, NUM_COLS);
	if(segment_number >= status.num_segments)
	{
		status.num_segments = segment_number + 1;
	}
	write_status_bar();
}

void set_segment_inactive(int32_t segment_number) {
	status.segments[segment_number].attrib = INACTIVE_STATUS_ATTRIB;
	write_status_bar();
}
void set_segment_active(int32_t segment_number) {
	status.segments[segment_number].attrib = ACTIVE_STATUS_ATTRIB;
	write_status_bar();
}

void status_click(int32_t x, int32_t y) {
	if (y != NUM_ROWS || x <= 0) {
		return;
	} else {
		int segment = 0;
		int screen_position = 0;
		for (segment = 0; segment < NUM_COLS; segment++) {
			if (screen_position == x) {
				// exactly at a separator; do nothing
				return;
			}
			if (screen_position > x) {
				segment--;
				break;
			}
			// add the segment length, +1 for the separator
			screen_position += status.segments[segment].length + 1;
		}

		if(segment == 0)
		{
			uint32_t flags;
			block_interrupts(&flags);
			process_t *new_process = kernel_spawn("shell");
			// We clicked on the "start" button.  Execute a new "shell" task!
			if (new_process == NULL) {
				// we were unable to start a shell, give up
				return;
			}
			switch_terminals(new_process->terminal);
			write_status_bar();
			restore_interrupts(flags);
		}

		int32_t terminal_num = segment - 2 + first_visible_segment;

		if(0 <= terminal_num && terminal_num <= NUM_TERMINALS &&
				status.segments[terminal_num].length > 0)
		{
			switch_terminals(&terminals[terminal_num]);
		}
		set_status_bar();
	}
}
