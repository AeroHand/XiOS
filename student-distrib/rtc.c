// vim: ts=4:sw=4:et
#include "rtc.h"
#include "i8259.h"
#include "interrupt.h"
#include "lib.h"
#include "keyboard.h"
#include "spinlock.h"


extern uint8_t cursor_on;
static uint32_t current_freq = 2;
static volatile uint32_t num_tics = 0;

static spinlock_t rtc_lock = SPINLOCK_UNLOCKED;

// the maximum available rate will be 2^MAX_FREQ_LOG2; we need rates for 2^0 ..
// 2^MAX_FREQ_LOG2
#define MAX_FREQ_LOG2 10
static uint32_t freq_users[MAX_FREQ_LOG2+1];

uint32_t log2(uint32_t num);

// forward-declarations
int32_t inc_users(int32_t freq);
int32_t dec_users(int32_t freq);
int32_t max_freq();

/** 
 * rtc event interrupt handler
 */
void rtc_handler(void)
{
    //save all
    registers_t regs;
    save_regs(regs);

    // regardless of current running frequency, update the cursor only at 2Hz
    if (num_tics % (current_freq/2) == 0) {
        cursor_on = !cursor_on;
    }
    update_cursor();
    //increment_video_mem();
    outb(0x0C, RTC_INDEX_PORT);
    inb(RTC_DATA_PORT);
    send_eoi(8);
    spin_lock(&rtc_lock);
    num_tics++;
    spin_unlock(&rtc_lock);
    //restore all
    restore_regs(regs);
    asm volatile ("   \
            leave       \n\
            iret"
            :
            :
            :"memory" );
}

/**
 * initialize the RTC hardware
 */
int32_t rtc_init(void)
{
    NMI_disable(); //disable all interrupts
    outb(0x0B, RTC_INDEX_PORT); //set index to register B
    uint8_t prev = inb(RTC_DATA_PORT); //read value in reg B
    outb(0x0B, RTC_INDEX_PORT); //set index back to B (read resets reg to D)
    outb(prev | 0x40, RTC_DATA_PORT); //turn on bit 6 of reg B
    rtc_set_freq(2);
    NMI_enable(); //enable all interrupts

    return 0;
}

/**
 * send a frequency to the RTC
 *
 * can only set the frequency to a power of 2
 */
int32_t rtc_set_freq(int32_t freq) {
    if(freq < 2 || freq > 1024) //if freq is out of allowed range
        return -1;
    if(freq & (freq -1)) //if freq is not power of 2
        return -1;

    spin_lock(&rtc_lock);
    current_freq = freq;
    spin_unlock(&rtc_lock);

    /*(freq = 32768 >> (rate-1))*/
    int rate = 1;
    int i;
    for(i=32768; i != freq; i >>= 1){rate++;}

    //printf("%d\n",rate);

    //set periodic interrupt rate
    NMI_disable(); //disable all interrupts
    outb(0x0A, RTC_INDEX_PORT); //set index to register A
    char prev=inb(RTC_DATA_PORT); //get initial value of register A
    outb(0x0A, RTC_INDEX_PORT); //reset index to A
    outb((prev & 0xF0) | rate, RTC_DATA_PORT); //write only our rate to A. Note, rate is the bottom 4 bits.
    NMI_enable(); //enable all interrupts

    return 0;
}

/**
 * open system call handler
 * 
 * just keeps track of one more user with rate of 2
 */
int32_t rtc_open(void)
{
    inc_users(2);
    rtc_set_freq(max_freq());

    return 0;
}

/**
 * read system call handler
 * 
 * returns after enough RTC interrupts have occurred
 *
 * the RTC file holds the desired frequency in its position field; the number
 * of tics to wait is computed based on the actual current rate of the RTC
 *
 * @param buf untouched
 * @return 0 after enough time has elapsed
 */
int32_t rtc_read(file_info_t *file, uint8_t* buf, int32_t length)
{
    uint32_t desired_freq = file->pos;
    if (desired_freq == 0) {
        desired_freq = 2;
    }
    uint32_t desired_tics = num_tics + current_freq/desired_freq;
    sti();
    while(num_tics < desired_tics){
        cli();
        schedule();
        sti();
    }
    cli();
    return 0;
}

/**
 * write system call handler
 *
 * change the RTC frequency
 *
 * @param buf the frequency to set to (as a buffer...)
 * @param nbytes the size of the argument; currently 1 means byte, 2 means
 * short and 4 means int/long
 * @return 0 if parameters are correct, -1 otherwise
 */
int32_t rtc_write(file_info_t *file, const int8_t* buf, int32_t nbytes)
{
    int32_t freq;
    if (buf == NULL || nbytes == 0) {
        return -1;
    }
    if (nbytes == 1) {
        freq = *buf;
    } else if (nbytes == 2) {
        freq = *(int16_t *)buf;
    } else if (nbytes == 4) {
        freq = *(int32_t *)buf;
    } else {
        return -1;
    }

    if (rtc_set_freq(freq) == 0) {
        dec_users(file->pos);
        file->pos = freq;
        inc_users(file->pos);
        return 0;
    } else {
        dec_users(file->pos);
        file->pos = 0;
        inc_users(file->pos);
        return -1;
    }
}

/**
 * close system call handler
 *
 * close the RTC, recording that that frequency's users have dropped by one
 * also will reduce the RTC frequency if appropriate
 */
int32_t rtc_close(file_info_t *file)
{
    dec_users(file->pos);
    //reset freq
    rtc_set_freq(max_freq());
    return 0;
}

/**
 * log base 2
 *
 * helper to take log base 2 of a number
 *
 * @return 0 if num is not a power of 2, log2(num) otherwise
 */
uint32_t log2(uint32_t num) {
    if (num & (num-1)) {
        // not a power of 2
        return 0;
    }
    int i;
    for (i = 1; i < MAX_FREQ_LOG2; i++) {
        if ( num == (1 << i) ) {
            return i;
        }
    }
    // too high, clamp to the max value
    return 1 << MAX_FREQ_LOG2;
}

/**
 * increment the number of users at a specified frequency
 */
int32_t inc_users(int32_t freq) {
    if (freq == 0) {
        freq = 2;
    }
    uint32_t index = log2(freq);
    return ++freq_users[index];
}

/**
 * decrement the number of users at a specified frequency
 */
int32_t dec_users(int32_t freq) {
    if (freq == 0) {
        freq = 2;
    }
    uint32_t index = log2(freq);
    return --freq_users[index];
}

/**
 * returns the maximum frequency with users; the minimum this function returns is 2
 */
int32_t max_freq() {
    int32_t i;
    for (i = MAX_FREQ_LOG2; i >= 1; i--) {
        if (freq_users[i] > 0) {
            return (1 << i);
        }
    }
    return 2;
}
