/* fdc.c - The floppy disk controller driver
 * vim:ts=4:sw=4:et
 */
#include "fdc.h"
#include "lib.h"
#include "i8259.h"

/* Floppy structure:
 * - 512B per sector
 * - 18 sector per track
 * - 80 tracks per side
 * - 2 sides
 *
 * - A cylinder is two tracks
 *     (One track on side 0, one on side 1)
 */

/* All fdc register information is available from
 *   http://www.isdaman.com/alsos/hardware/fdc/floppy.htm
 */

//buffer is equal to 1 cylinder (18,432B)
static uint8_t fdc_dmabuffer[FDC_BUFFER_SIZE] __attribute__((aligned(0x8000)));

static volatile fdc_motor_state_t motor_state = MOTOR_OFF;
static volatile uint32_t fdc_interrupt_occurred = 0;
static volatile int32_t fdc_drive = -1;

static const int8_t* drive_types[8] = {
    "none",
    "360kB 5.25\"",
    "1.2MB 5.25\"",
    "720kB 3.5\"",

    "1.44MB 2.5\"",
    "2.88MB 3.5\"",
    "unknown type",
    "unknown type"
};

void fdc_write_cmd(uint8_t cmd);
uint8_t fdc_read_data(void);
void fdc_awk(ST0_byte_t* st0, uint32_t* cyl);
int32_t fdc_calibrate(void);
void fdc_motor(fdc_motor_state_t set_state);
void fdc_irq_wait(void);
void fdc_sleep(void);
int32_t fdc_seek(uint32_t cylinder, uint32_t head);
void fdc_dma_init(fdc_direction_t dir);
int32_t fdc_do_track(uint32_t cylinder, fdc_direction_t dir);
int32_t fdc_check_error(void);
void fdc_motor(fdc_motor_state_t set_state);
int32_t fdc_reset(uint32_t drive);

int32_t fdc_init(uint32_t drive) {
    int32_t ret;
    if(drive > 1) {
        return -1;
    }
    //set the current drive number
    fdc_drive = (int32_t) drive;

    ret = fdc_reset(drive);
    if(ret != 0) {
        fdc_drive = -1;
    }
    return ret;

}

void fdc_detect_drives(void) {
    //CMOS inquiry
    outb(0x10, 0x70);
    uint32_t drives = inb(0x71);

    printf(" - Floppy drive 0: %s\n", drive_types[drives >> 4]);
    printf(" - Floppy drive 1: %s\n", drive_types[drives & 0xf]);

    return;
}

void fdc_write_cmd(uint8_t cmd) {
    while(1) {
        MSR_byte_t msr;
        msr.val = inb(FDC_REG_BASE + REG_MSR);
        if(msr.mrq == 1 && msr.dio == 0)
        {
            outb(cmd, FDC_REG_BASE + REG_FIFO);
            return;
        }
    }
}

uint8_t fdc_read_data(void) {
    while(1) {
        MSR_byte_t msr;
        msr.val = inb(FDC_REG_BASE + REG_MSR);
        if(msr.mrq == 1)
        {
            return inb(FDC_REG_BASE + REG_FIFO);
        }
    }
}

void fdc_awk(ST0_byte_t* st0, uint32_t* cyl) {
    fdc_write_cmd(CMD_CHECK_INTERRUPT_STATUS);
    st0->val = fdc_read_data();
    *cyl = fdc_read_data();
}

/* Positions r/w head to cylinder 0.
 *   - Sets DIR signal to 0
 *   - Passes up to 79 step pulses toward track 0
 *   - Checks TRK0 signal
 */
int32_t fdc_calibrate(void) {
    ST0_byte_t st0;
    uint32_t cyl;
    fdc_motor(MOTOR_ON);
    while(1) {
        fdc_write_cmd(CMD_CALIBRATE);
        fdc_write_cmd(fdc_drive); //drive number {0, 1, 2, 3}

        fdc_irq_wait();
        fdc_awk(&st0, &cyl);

        if(st0.ic) {
            int8_t* status[] =
                { 0, "error", "invalid", "drive" };
            printf("floppy_calibrate: status = %s\n", status[st0.ic]);
            continue;
        }

        if(cyl == 0) {
            fdc_motor(MOTOR_OFF);
            return 0;
        }
    }
    fdc_motor(MOTOR_OFF);
    return -1;
}

int32_t fdc_reset(uint32_t drive) {
    ST0_byte_t st0;
    uint32_t cyl;
    DOR_byte_t dor;
    CCR_byte_t ccr;

    //set dor to reset byte
    dor.val = 0x00;
    outb(dor, FDC_REG_BASE + REG_DOR);

    //select DMA and non-reset
    {
        dor.dma = 1;
        dor.nrst = 1;
    }
    outb(dor, FDC_REG_BASE + REG_DOR);

    fdc_irq_wait();
    fdc_awk(&st0, &cyl);

    {
        ccr.reserved = 0;
        ccr.rate = 0; // 500kb/s
    }
    outb(ccr, FDC_REG_BASE + REG_CCR);
    
    //send specific physical information for disk drive
    //  byte1[7:4] = step rate
    //  byte1[3:0] = head unload time
    //  byte2[7:1] = head load time
    //  byte2[0] = 0-DMA, 1-noDMA
    fdc_write_cmd(CMD_FIX_DRIVE_DATA);
    fdc_write_cmd(0xdf);
    fdc_write_cmd(0x02);

    if(fdc_calibrate()) {
        return -1;
    }
    return 0;
}

void fdc_motor(fdc_motor_state_t set_state) {
    DOR_byte_t dor;
    dor.val = 0x00;
    {
        dor.dsel = fdc_drive;
        dor.nrst = 1;
        dor.dma = 1;
        if(fdc_drive == 0) {
            dor.mota = 1;
        } else if(fdc_drive == 1) {
            dor.motb = 1;
        } else if(fdc_drive == 2) {
            dor.motc = 1;
        } else if(fdc_drive == 3) {
            dor.motd = 1;
        }
    }
    if(set_state == MOTOR_ON) {
        if(motor_state == MOTOR_OFF) {
            //turn on
            outb(dor, FDC_REG_BASE + REG_DOR);
            fdc_sleep();
        }
        motor_state = MOTOR_ON;
    } else {
        //turn it off, hopefully
        {
            dor.mota = 0;
            dor.motb = 0;
            dor.motc = 0;
            dor.motd = 0;
        }
        outb(dor, FDC_REG_BASE + REG_DOR);
        motor_state = MOTOR_OFF;
    }
}

void fdc_irq_wait(void) {
    sti();
    while(!fdc_interrupt_occurred) {};
    cli();
    fdc_interrupt_occurred = 0;
    return;
}

void fdc_handler(void) {
    registers_t regs;
    save_regs(regs);
    
    fdc_interrupt_occurred = 1;

    send_eoi(FDC_IRQ);

    restore_regs(regs);
    asm volatile ("       \
        leave           \n\
        iret"
        :
        :
        :"memory" );
}

void fdc_sleep(void) {
    //wait for 500ms...
    // or not. we have a VM
}

int32_t fdc_seek(uint32_t cylinder, uint32_t head) {
    ST0_byte_t st0;
    uint32_t cyl;

    fdc_motor(MOTOR_ON);

    while(1) {
        fdc_write_cmd(CMD_SEEK);
        fdc_write_cmd(head<<2 | fdc_drive);
        fdc_write_cmd(cylinder);

        fdc_irq_wait();
        fdc_awk(&st0, &cyl);

        if(st0.ic) {
            int8_t* status[] =
                { "normal", "error", "invalid", "drive" };
            printf("floppy_seek: status = %s\n", status[st0.ic]);
            continue;
        }

        if(cyl == cylinder) {
            fdc_motor(MOTOR_OFF);
            return 0;
        }
    }

    printf("floppy_seek: retries exhausted\n");
    fdc_motor(MOTOR_OFF);
    return -1;
}

void fdc_dma_init(fdc_direction_t dir) {
    union {
        uint8_t b[4]; //4 bytes
        uint32_t l;   //1 long
    } a, c; //address and count

    a.l = (uint32_t) &fdc_dmabuffer;
    c.l = (uint32_t) FDC_BUFFER_SIZE - 1; // -1 for DMA counting

    if((a.l >> 24) || (c.l >> 16) || (((a.l & 0xffff) + c.l) >> 16)) {
        printf("fdc_dma_init: dma buffer error\n");
        return;
    }

    //DMA setup code
    outb(0x06, 0x0a); //mask channel 2
    outb(0xff, 0x0c); //reset flip-flop
    outb(a.b[0], 0x04); // address low byte
    outb(a.b[1], 0x04); // address high byte
    outb(a.b[2], 0x81); // external page register
    outb(0xff, 0x0c); //reset flip-flop
    outb(c.b[0], 0x05); // count low byte
    outb(c.b[1], 0x05); // count high byte

    if(dir == FDC_READ) { //set mode
        // 01:0:0:01:10 = single/inc/no-auto/to-mem/channel2
        outb(0x46, 0x0b);
    } else if(dir == FDC_WRITE) {
        // 01:0:0:10:10 = single/inc/no-auto/from-mem/channel2
        outb(0x4a, 0x0b); 
    } else {
        printf("fdc_dma_init: invalid direction\n");
        return;
    }
        
    outb(0x02, 0x0a); //unmask channel 2
}

//reads a full cylinder (32kB)
int32_t fdc_do_track(uint32_t cylinder, fdc_direction_t dir) {
    uint32_t cmd, error;
    //cmd is MT:MF:SK:fdc_command(read/write)
    // MT-multitrack mode
    //   controller will automatically switch from head 0
    //   to head 1 at the end of a track
    // MF-MFM mode
    //   magnetic encoding mode
    //   always set for read/write/format/verify operations
    // SK-skip deleted
    //   very strange fuctionality, leave cleared
    uint32_t flags = 0xc0;

    if(dir == FDC_READ) {
        cmd = CMD_READ_DATA | flags;
    } else if(dir == FDC_WRITE) {
        cmd = CMD_WRITE_DATA | flags;
    } else {
        printf("fdc_do_track: invalid direction\n");
        return 0; 
    }

    // seek both heads
    //   some docs say you only need to seek one head
    if(fdc_seek(cylinder, 0)) {
        return -1;
    }
    if(fdc_seek(cylinder, 1)) {
        return -1;
    }

    while(1) {
        fdc_motor(MOTOR_ON);
        fdc_dma_init(dir);
        fdc_sleep(); //wait after the seeks
        
        fdc_write_cmd(cmd); //set direction
        fdc_write_cmd(0x00 | fdc_drive); //0:0:0:0:0:HD:US1:US0 = head and drive
        fdc_write_cmd(cylinder); //set cylinder
        fdc_write_cmd(0x00); //set head
        fdc_write_cmd(0x01); //set sector (counts from 1)
        fdc_write_cmd(0x02); //sector size, 128*2^x (512B)
        fdc_write_cmd(0x12); //track length (18)
        fdc_write_cmd(0x1b); //GAP3 length (27B is default for 3.5")
        fdc_write_cmd(0xff); //data length

        fdc_irq_wait();
        error = fdc_check_error();

        if(!error) {
            fdc_motor(MOTOR_OFF);
            return 0;
        } else if(error > 1) {
            printf("fdc_do_track: fatal sector error\n");
            return -2;
        }
    }

    printf("fdc_do_track: retries exhausted\n");
    fdc_motor(MOTOR_OFF);
    return -1;
}

int32_t fdc_check_error(void) {
    ST0_byte_t st0;
    ST1_byte_t st1;
    ST2_byte_t st2;
    uint8_t rcyl, rhead, rsec, bps;
    st0.val = fdc_read_data();
    st1.val = fdc_read_data();
    st2.val = fdc_read_data();
    //updated cylinder, head, sector values
    rcyl = fdc_read_data();
    rhead = fdc_read_data();
    rsec = fdc_read_data();
    bps = fdc_read_data(); //bytes per second

    if(st0.ic) {
        int8_t * status[] =
            { 0, "error", "invalid command", "drive not ready" };
        printf("fdc_do_track: status = %s\n", status[st0.ic]);
        return 1;
    }
    if(st1.en) {
        printf("fdc_do_track: end of cylinder\n");
        return 1;
    }
    if(st0.nr) {
        printf("fdc_do_track: drive not ready\n");
        return 1;
    }
    if(st1.de) {
        printf("fdc_do_track: error in ID address field\n");
        return 1;
    }
    if(st1.to) {
        printf("fdc_do_track: controller timeout\n");
        return 1;
    }
    if(st1.ndat) {
        printf("fdc_do_track: no data found\n");
        return 1;
    }
    if(st1.nid | st2.ndam) {
        printf("fdc_do_track: no address mark found\n");
        return 1;
    }
    if(st2.dadm) {
        printf("fdc_do_track: deleted address mark\n");
        return 1;
    }
    if(st2.crce) {
        printf("fdc_do_track: CRC error in data\n");
        return 1;
    }
    if(st2.wcyl) {
        printf("fdc_do_track: wrong cylinder\n");
        return 1;
    }
    if(st2.serr) {
        printf("fdc_do_track: uPD765 sector not found\n");
        return 1;
    }
    if(st2.bcyl) {
        printf("fdc_do_track: bad cylinder\n");
        return 1;
    }
    if(bps != 0x02) {
        printf("fdc_do_track: wanted 512B/sector, got %d\n", (1<<(bps+7)));
        return 1;
    }
    if(st1.nw) {
        printf("fdc_do_track: not writable\n");
        return 2;
    }
    return 0;
}

/* Write from (buffer) onto the floppy.
 * This writes from the BEGINNING of the disk.
 */
int32_t fdc_disk_write(uint8_t* buffer, uint32_t bytes) {
    uint32_t cylinder, pos, remaining, copy_this_iter, ret;
    if(fdc_drive < 0) {
        return -1;
    }
    cylinder = 0;
    pos = 0;
    remaining = bytes;
    copy_this_iter = FDC_BUFFER_SIZE;
    if(bytes > FDC_MAX_SIZE) {
        return -1;
    }
    while(remaining > 0) {
        if(remaining < FDC_BUFFER_SIZE) {
            copy_this_iter = remaining;
        }
        memcpy(&fdc_dmabuffer, buffer + pos, copy_this_iter);
        ret = fdc_do_track(cylinder, FDC_WRITE);
        if(ret != 0) {
            return ret;
        }
        pos += copy_this_iter;
        remaining -= copy_this_iter;
        cylinder++;
    }
    return 0;
}

/* Read from the floppy into (buffer)/
 * This reads from the BEGINNING of the disk.
 */
int32_t fdc_disk_read(uint8_t* buffer, uint32_t bytes) {
    int32_t cylinder, pos, remaining, copy_this_iter, ret;
    if(fdc_drive < 0) {
        return -1;
    }
    cylinder = 0;
    pos = 0;
    remaining = bytes;
    copy_this_iter = FDC_BUFFER_SIZE;
    if(bytes > FDC_MAX_SIZE) {
        return -1;
    }
    while(remaining > 0) {
        if(remaining < FDC_BUFFER_SIZE) {
            copy_this_iter = remaining;
        }
        ret = fdc_do_track(cylinder, FDC_READ);
        if(ret != 0) {
            return ret;
        }
        memcpy(buffer + pos, &fdc_dmabuffer, copy_this_iter);
        pos += copy_this_iter;
        remaining -= copy_this_iter;
        cylinder++;
    }
    return 0;
}
