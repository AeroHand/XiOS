//vim:ts=4:sw=4:et
#ifndef __INTERRUPT_H
#define __INTERRUPT_H

/**
 * Function to set up interrupts for the system.
 * Only needs to be called once upon boot.
 */
int init_interrupts(void);

/**
 * Function to enable non-maskable interrupts.
 */
void NMI_enable(void);
/**
 * Function to disable non-maskable interrupts.
 */
void NMI_disable(void);
#endif
