// vim:ts=4:sw=4:et
#include "x86_desc.h"
#include "lib.h"
#include "keyboard.h"
#include "i8259.h"
#include "rtc.h"
#include "interrupt.h"
#include "syscall.h"
#include "pit.h"
#include "mouse.h"
#include "sb16.h"
#include "fdc.h"

/**
 * Function to set up interrupts for the system.
 * Only needs to be called once upon boot.
 */
int init_interrupts(void)
{
    // Set up generic interrupt gate.
    idt_desc_t interrupt;
    interrupt.present = 1;
    interrupt.dpl = 0;
    interrupt.reserved0 = 0;
    interrupt.size = 1;
    interrupt.reserved1 = 1;
    interrupt.reserved2 = 1;
    interrupt.reserved3 = 0;
    interrupt.reserved4 = 0;
    interrupt.seg_selector = KERNEL_CS;

    // Use test_interrupts to handle all interrupts (checkpoint 1)
    SET_IDT_ENTRY(interrupt, test_interrupts);

    /* exception idt_dest_t is almost identical to interrupt,
     * except reserved3 needs to be 1 */
    idt_desc_t exception = interrupt;
    exception.reserved3 = 1;
    SET_IDT_ENTRY(exception, ex_reserved);

    // assign separate handler for keyboard
    idt_desc_t keypress = interrupt;
    SET_IDT_ENTRY(keypress, keypress_handler);

    // assign separate handler for rtc
    idt_desc_t rtc = interrupt;
    SET_IDT_ENTRY(rtc, rtc_handler);

    // assign separate handler for pit
    idt_desc_t pit = interrupt;
    SET_IDT_ENTRY(pit, pit_handler);

    // assign separate handler for mouse
    idt_desc_t mouse = interrupt;
    SET_IDT_ENTRY(mouse, mouse_handler);

    // assign separate handler for SB16
    idt_desc_t sb16 = interrupt;
    SET_IDT_ENTRY(sb16, sb16_handler);

    // assign separate handler for fdc
    idt_desc_t fdc = interrupt;
    SET_IDT_ENTRY(fdc, fdc_handler);

    // assign separate handler for syscall
    idt_desc_t syscall = interrupt;
    syscall.dpl = 3;
    SET_IDT_ENTRY(syscall, syscall_handler);

    // set the idt entry for syscall
    idt[0x80] = syscall;

    // set all idt entries for exceptions and interrupts
    int i;
    for (i = 0; i < 0x20; i++) {
        idt[i] = exception;
    }	
    for (i = 0x20; i < 0x2F; i++) {
        idt[i] = interrupt;
    }

    //set up individual exception handlers
    SET_IDT_ENTRY(idt[0], ex_divide_error);
    SET_IDT_ENTRY(idt[1], ex_debug);
    SET_IDT_ENTRY(idt[2], ex_NMI);
    SET_IDT_ENTRY(idt[3], ex_breakpoint);
    SET_IDT_ENTRY(idt[4], ex_overflow);
    SET_IDT_ENTRY(idt[5], ex_bound_range);
    SET_IDT_ENTRY(idt[6], ex_invalid_op);
    SET_IDT_ENTRY(idt[7], ex_device_na);
    SET_IDT_ENTRY(idt[8], ex_double_fault);
    SET_IDT_ENTRY(idt[9], ex_segment_overrun);
    SET_IDT_ENTRY(idt[10], ex_invalid_TSS);
    SET_IDT_ENTRY(idt[11], ex_no_segment);
    SET_IDT_ENTRY(idt[12], ex_seg_fault);
    SET_IDT_ENTRY(idt[13], ex_gen_protection);
    SET_IDT_ENTRY(idt[14], ex_page_fault);
    SET_IDT_ENTRY(idt[15], ex_reserved);
    SET_IDT_ENTRY(idt[16], ex_float_pt_err);
    SET_IDT_ENTRY(idt[17], ex_align_check);
    SET_IDT_ENTRY(idt[18], ex_machine_check);
    SET_IDT_ENTRY(idt[19], ex_SIMD_float_pt);
    for(i=20; i <= 31; i++) {
        SET_IDT_ENTRY(idt[i], ex_reserved);		
    }

    // init keyboard
    idt[0x21] = keypress;
    enable_irq(1);

    // init rtc
    idt[0x28] = rtc;
    enable_irq(8);

    // init pit
    idt[0x20] = pit;
    enable_irq(0);

    // init mouse
    idt[0x20 + 12] = mouse;
    enable_irq(12);

    // init sb16
    idt[0x25] = sb16;
    enable_irq(5);

    //init fdc
    idt[0x26] = fdc;
    enable_irq(6);

    // Load IDT.
    lidt(idt_desc_ptr);

    return 0;
}

/**
 * Function to enable non-maskable interrupts.
 */
void NMI_enable(void)
{
    //sti();
    uint8_t prev = inb(0x70);
    outb(prev & 0x7F, 0x70);
}

/**
 * Function to disable non-maskable interrupts.
 */
void NMI_disable(void)
{
    cli();
    uint8_t prev = inb(0x70);
    outb(prev | 0x80, 0x70);
}

