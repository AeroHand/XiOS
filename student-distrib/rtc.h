/* rtc.h - Defines functions used in interactions with the rtc controller
 * vim:ts=4:sw=4:et
 */

#ifndef _RTC_H
#define _RTC_H

#include "types.h"
#include "fs.h"

/* Ports that each PIC sits on */
#define RTC_INDEX_PORT 0x70
#define RTC_DATA_PORT  0x71

/* Externally-visible functions */

// Initialize rtc and set to 2Hz
int32_t rtc_init();
// Open rtc
int32_t rtc_open(void);
// rtc interrupt handler
void rtc_handler(void);
int32_t rtc_set_freq(int32_t freq);
// returns upon next rtc interrupt
int32_t rtc_read(file_info_t *file, uint8_t* buf, int32_t length);
// sets rtc to user-defined frequency, returns 0 on success
int32_t rtc_write(file_info_t *file, const int8_t* buf, int32_t nbytes);
int32_t rtc_close(file_info_t *file);

#endif /* _RTC_H */
