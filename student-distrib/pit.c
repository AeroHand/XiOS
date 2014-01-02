//vim:ts=4:sw=4:et
#include "pit.h"
#include "lib.h"
#include "i8259.h"
#include "interrupt.h"
#include "task.h"

volatile int pit_interrupt_occurred = 0;

/**
 * low-level interface to configure the PIT
 *
 * @param channel
 * - 0: system timer
 * - 2: speaker
 * @param mode
 * - 2: rate generator (for periodic interrupts)
 * - 3: square wave (for audio signals)
 * @freq frequency a number between 19 and 1193182 Hz
 * @return 0 if parameters were correct, -1 on error
 * 
 */
int pit_config(int channel, int mode, int freq)
{
    short rate = 0;
    short data = 0;

    // limiting to forseably usable channels/modes
    if(channel != 0 && channel != 2) return -1;
    if(mode != 2 && mode != 3) return -1;

    if(freq < PIT_MIN_FREQ) rate = 0;
    else if(freq > PIT_MAX_FREQ) rate = 2;
    else rate = (PIT_MAX_FREQ + freq /2) / freq;

    data = (channel << 6) | 0x30 | (mode << 1);

    NMI_disable();
    outb(data, PIT_CMD_PORT);
    outb(rate, PIT_DATA_PORT(channel));
    outb(rate >> 8, PIT_DATA_PORT(channel));
    NMI_enable();

    return 0;
}

/**
 * start the timer
 *
 * helper that configures the PIT to generate periodic interrupts at a specified frequency
 *
 * @return 0 if frequency was valid and -1 if out of bounds
 */
int timer_start(int freq)
{
    return pit_config(0, 2, freq);
}

/**
 * blocks while waiting for PIT interrupt
 */
void pit_read(void)
{
    cli();
    pit_interrupt_occurred = 0;
    sti();
    while(!pit_interrupt_occurred){}
    return;
}

/**
 * interrupt handler for PIT
 *
 * currently does only scheduling
 */
void pit_handler(void)
{
    //save all
    registers_t regs;
    save_regs(regs);

    pit_interrupt_occurred = 1;

    send_eoi(0);

    //call scheduler stuff
    schedule();

    //restore all
    restore_regs(regs);
    asm volatile ("   \
            leave       \n\
            iret"
            :
            :
            :"memory" );
}
