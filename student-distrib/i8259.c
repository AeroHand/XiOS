/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4:sw=4:et
 **/

#include "i8259.h"
#include "lib.h"

/* Interrupt masks to determine which interrupts
 * are enabled and disabled */
uint8_t master_mask; /* IRQs 0-7 */
uint8_t slave_mask; /* IRQs 8-15 */

/**
 * Initialize the 8259 PIC. 
 */
    void
i8259_init(void)
{
    // send commands to the PIC to initialize it

    // mask everything, lol
    master_mask = 0xFF;
    slave_mask = 0xFF;
    send_masks();

    // Initialize master PIC.
    outb(ICW1, MASTER_8259_PORT);  
    outb(ICW2_MASTER, MASTER_8259_PORT + 1);
    outb(ICW3_MASTER, MASTER_8259_PORT + 1);
    outb(ICW4, MASTER_8259_PORT + 1);

    // Initialize slave PIC.
    outb(ICW1, SLAVE_8259_PORT); 
    outb(ICW2_SLAVE, SLAVE_8259_PORT + 1);
    outb(ICW3_SLAVE, SLAVE_8259_PORT + 1);
    outb(ICW4, SLAVE_8259_PORT + 1);

    // Wait for the PIC to initialize.
    int i;
    int test[1000];
    int test2[1000];
    for(i=0; i<1000; i++)
    {
        test[i] = test2[i];
    }
}

/**
 * Enable (unmask) the specified IRQ 
 */
    void
enable_irq(uint32_t irq_num)
{
    if (irq_num < 8) {
		// Enable the IRQ on the master PIC.
        master_mask &= ~(0x01 << irq_num); 
    }
    else if (irq_num < 16){
		// Enable the IRQ on the slave PIC.
        slave_mask &= ~(0x01 << (irq_num - 8));
		// Enable the master PIC.
        enable_irq(2);
    }
	// Send the updated masks to the PICs.
    send_masks();
}

/**
 * Disable (mask) the specified IRQ 
 */
    void
disable_irq(uint32_t irq_num)
{
    if (irq_num < 8) {
		// Disable the IRQ on the master PIC.
        master_mask |= (0x01 << irq_num); 
    }
    else if (irq_num < 16){
		// Disable the IRQ on the slave PIC.
        slave_mask |= (0x01 << (irq_num - 8));
		// If we've disabled all of the slave PIC's interrupts,
		// disable the slave PIC itself as well.
        if (slave_mask == 0xFF) {
            disable_irq(2);
        }
    }
	// Send the updated masks to the PICs.
    send_masks();
}

/**
 * Send end-of-interrupt signal for the specified IRQ 
 */
    void
send_eoi(uint32_t irq_num)
{
    if (irq_num < 8) {
		// Send EOI to the master PIC.
        outb((EOI | irq_num), MASTER_8259_PORT);
    } else if (irq_num < 16) {
		// Send EOI to the slave PIC.
        outb((EOI | (irq_num - 8)), SLAVE_8259_PORT);
		// Send EOI to the master PIC (for the "slave PIC" line).
        send_eoi(2);
    }
}

/**
 * Actually send the given bitmasks to the PIC.
 */
void send_masks(void)
{
    outb(master_mask, MASTER_8259_PORT + 1);
    outb(slave_mask, SLAVE_8259_PORT + 1);
}

