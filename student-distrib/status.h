#ifndef _STATUS_H
#define _STATUS_H

#include "lib.h"
#include "task.h"
#include "keyboard.h"

#define INACTIVE_STATUS_ATTRIB (FORE(BRIGHT(GREEN)) | BACK(BLACK))
#define ACTIVE_STATUS_ATTRIB (FORE(BRIGHT(BLUE)) | BACK(BRIGHT(GREEN)))

typedef struct{
	int8_t data[NUM_COLS];
	int32_t length;
	uint8_t attrib;	
} status_segment_t;

typedef struct{
	status_segment_t segments[NUM_COLS];
	int32_t num_segments;
	int32_t active_segment;
} status_bar_t;

extern status_bar_t status;

void init_status(void);
int32_t write_status_bar(void);
void write_status_char(int8_t theChar, int8_t current_attrib, uint32_t x);
void set_segment_data(int32_t segment_number, int8_t* data);
void status_click(int32_t x, int32_t y);
void set_segment_inactive(int32_t segment_number);
void set_segment_active(int32_t segment_number);

#endif
