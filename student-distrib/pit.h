/* rtc.h - Defines functions used in interactions with the rtc controller
 * vim:ts=4:sw=4:et
 */

#ifndef _PIT_H
#define _PIT_H

#include "types.h"

#define PIT_MAX_FREQ 1193182
#define PIT_MIN_FREQ 19

#define PIT_CMD_PORT  0x43
#define PIT_DATA_PORT(CHANNEL) (0x40 + (CHANNEL))

int pit_config(int channel, int mode, int freq);

int timer_start(int freq);

void pit_read(void);

void pit_handler(void);

#endif /* _PIT_H */
