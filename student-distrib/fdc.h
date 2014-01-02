//vim:ts=4:sw=4:et
#ifndef _FDC_H
#define _FDC_H

#include "types.h"

#define FDC_MAX_SIZE 1474560
#define FDC_BUFFER_SIZE 0x4800
#define FDC_REG_BASE 0x3f0
#define FDC_IRQ 6

enum fdc_registers {
    REG_DOR = 2,
    REG_MSR = 4,
    REG_FIFO = 5,
    REG_CCR = 7
};

enum fdc_commands {
    CMD_FIX_DRIVE_DATA = 3, //(SPECIFY)
    CMD_WRITE_DATA = 5,
    CMD_READ_DATA = 6,
    CMD_CALIBRATE = 7,
    CMD_CHECK_INTERRUPT_STATUS = 8,
    CMD_SEEK = 15, //seeks both heads to a cylinder
    CMD_PERPENDICULAR_MODE = 18
};

typedef enum fdc_motor_state {
    MOTOR_OFF = 0,
    MOTOR_ON = 1,
    MOTOR_WAIT = 2
} fdc_motor_state_t;

typedef enum fdc_direction {
    FDC_READ = 1,
    FDC_WRITE = 2
} fdc_direction_t;

//main status register
typedef struct MSR_byte {
    union {
        uint8_t val;
        struct {
            //drive {0,1,2,3} is seeking
            uint32_t acta : 1;
            uint32_t actb : 1;
            uint32_t actc : 1;
            uint32_t actd : 1;
            //set when cmd byte recieved, cleared at the end of Result
            uint32_t busy : 1;
            uint32_t ndma : 1; //set in exec phase of PIO r/w commands
            uint32_t dio : 1; //set if FIFO IO expects INPUT
            uint32_t mrq : 1; //set if OK to use FIFO IO port
        } __attribute__((packed));
    };
} MSR_byte_t;

//digital output register
typedef struct DOR_byte {
    union {
        uint8_t val;
        struct {
            //drive select for next access
            uint32_t dsel : 2;
            //0=enter reset mode, 1=normal operation
            uint32_t nrst : 1;
            //enable IRQs and DMA
            uint32_t dma : 1;
            //set to turn drive {0,1,2,3} motor ON
            uint32_t mota : 1;
            uint32_t motb : 1;
            uint32_t motc : 1;
            uint32_t motd : 1;
        } __attribute__((packed));
    };
} DOR_byte_t;

//status register 0
typedef struct ST0_byte {
    union {
        uint8_t val;
        struct {
            //currently selected drive
            uint32_t us : 2; //unit select
            uint32_t hd : 1; //active head
            uint32_t nr : 1; //drive not ready
            //set if drive faults or recalibrate cannot find
            //  track 0 after 79 pulses
            uint32_t uc : 1; //unit check
            //fdc completed seek or calibration command
            //  or has correctly execcuted a read/write command
            //  which had an implicit seek
            uint32_t se : 1; //seek end
            //00- normal termination
            //01- abnormal termination
            //10- invalid command
            //11- abnormal termination by polling
            uint32_t ic : 2; //interrupt code
        } __attribute__((packed));
    };
} ST0_byte_t;

//status register 1
typedef struct ST1_byte {
    union {
        uint8_t val;
        struct {
            uint32_t nid : 1; //no address mark
            uint32_t nw : 1; //not writable (disk protected)
            uint32_t ndat : 1; //no data
            uint32_t res3 : 1; //always 0
            uint32_t to : 1; //time-out
            uint32_t de : 1; //data error
            uint32_t res6 : 1; //always 0
            //set when sector count exceeds number of sectors on track
            uint32_t en : 1; //end of cylinder
        } __attribute__((packed));
    };
} ST1_byte_t;

//status register 2
typedef struct ST2_byte {
    union {
        uint8_t val;
        struct {
            uint32_t ndam : 1; //not data address mark DAM
            uint32_t bcyl : 1; //bad cylinder
            uint32_t serr : 1; //seek error (if uPD765)
            uint32_t seq : 1; //seek equal (if uPD765)
            //set if track address in controller and in the ID
            //  address mark are different
            uint32_t wcyl : 1; //wrong cylinder
            uint32_t crce : 1; //CRC error in data field
            uint32_t dadm : 1; //deleted address mark
            uint32_t res7 : 1; //always 0
        } __attribute__((packed));
    };
} ST2_byte_t;

//status register 3
typedef struct ST3_byte {
    union {
        uint8_t val;
        struct {
            uint32_t ds : 2; //drive select
            uint32_t hddr : 1; //head
            uint32_t dsdr : 1; //double sided drive
            uint32_t trk0 : 1; //track 0;
            uint32_t rdy : 1; //ready
            uint32_t wpdr : 1; //write protection (set if protected)
            uint32_t esig : 1; //error occurred (if uPD765)
        } __attribute__((packed));
    };
} ST3_byte_t;

//control configuration register
typedef struct CCR_byte {
    union {
        uint8_t val;
        struct {
            uint32_t reserved : 6;
            //data transfer rate
            uint32_t rate : 2;
        } __attribute__((packed));
    };
} CCR_byte_t;

int32_t fdc_init(uint32_t drive);
int32_t fdc_disk_write(uint8_t* buffer, uint32_t bytes);
int32_t fdc_disk_read(uint8_t* buffer, uint32_t bytes);
void fdc_detect_drives(void);
void fdc_handler(void);

#endif /* _FDC_H */
