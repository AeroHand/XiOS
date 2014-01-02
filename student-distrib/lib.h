/* lib.h - Defines for useful library functions
 * vim:ts=4:sw=4:et
 */

#ifndef _LIB_H
#define _LIB_H

#include "types.h"
#include "x86_desc.h"

#define NUM_COLS 80
#define NUM_ROWS 24
#define MB(num) ((num) << 20)
#define KB(num) ((num) << 10)
#define VIDEO 0xB8000

#ifdef NDEBUG
#define assert(ignore)((void) 0)
#else
void abort(char* exp, char* file, int line);
#define assert(exp) if(exp) ; \
    else abort(#exp, __FILE__, __LINE__)
#endif

extern uint8_t cursor_on;
extern uint8_t current_attrib;

typedef struct{
    uint8_t x;
    uint8_t y;
} coord_t;

typedef void (*attrib_updated_t)(int32_t x, int32_t y);

void set_screen_coordinates(int x, int y);
void set_attrib(uint8_t attrib);
void add_attrib_observer(attrib_updated_t f);
void set_char_attrib(int x, int y, uint8_t attrib);
uint8_t get_char_attrib(int x, int y);
void set_cursor_position(int x, int y);
void clear_char_attrib(int x, int y);
coord_t read_screen_coordinates(void);

int32_t printf(int8_t *format, ...);
void putc(uint8_t c);
int32_t puts(int8_t *s);
int32_t puts_wrap(int8_t *s);
int32_t putns_wrap(int8_t *s, int32_t nbytes);
int8_t *itoa(uint32_t value, int8_t* buf, int32_t radix);
int8_t *strrev(int8_t* s);
uint32_t strlen(const int8_t* s);
void clear(void);
void clear_line(int8_t start_x, int8_t line);
void scroll(void);

void* memset(void* s, int32_t c, uint32_t n);
void* memset_word(void* s, int32_t c, uint32_t n);
void* memset_dword(void* s, int32_t c, uint32_t n);
void* memcpy(void* dest, const void* src, uint32_t n);
void* memmove(void* dest, const void* src, uint32_t n);
uint32_t strlcat(int8_t *dest, const int8_t *src, uint32_t size);
int32_t strncmp(const int8_t* s1, const int8_t* s2, uint32_t n);
int32_t strcmp(const char* s1, const char* s2);
int32_t substr(const char* s1, const char* s2);
int8_t* strcpy(int8_t* dest, const int8_t*src);
int8_t* strncpy(int8_t* dest, const int8_t*src, uint32_t n);

void update_cursor(void);

/* Userspace address-check functions */
int32_t bad_userspace_addr(const void* addr, int32_t len);
int32_t safe_strncpy(int8_t* dest, const int8_t* src, int32_t n);

/* Port read functions */
/* Inb reads a byte and returns its value as a zero-extended 32-bit
 * unsigned int */
static inline uint32_t inb(port)
{
    uint32_t val;
    asm volatile ("         \
        xorl %0, %0       \n\
        inb   (%w1), %b0" 
        : "=a"(val)
        : "d"(port)
        : "memory" );
    return val;
} 

/* Reads two bytes from two consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them zero-extended
 * */
static inline uint32_t inw(port)
{
    uint32_t val;
    asm volatile ("         \
        xorl %0, %0       \n\
        inw   (%w1), %w0"
        : "=a"(val)
        : "d"(port)
        : "memory" );
    return val;
}

/* Reads four bytes from four consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them */
static inline uint32_t inl(port)
{
    uint32_t val;
    asm volatile ("     \
        inl   (%w1), %0"
        : "=a"(val)
        : "d"(port)
        : "memory" );
    return val;
}

void increment_video_mem(void);
void increment_video_location(int col, int row);

void init_graphics(void);
void bsod(void);
void shutdown_screen(void);
void printSB(void);
void printXiOS(void);
void startup_screen(void);

//exception handlers
void ex_divide_error(void);
void ex_debug(void);
void ex_NMI(void);
void ex_breakpoint(void);
void ex_overflow(void);
void ex_bound_range(void);
void ex_invalid_op(void);
void ex_device_na(void);
void ex_double_fault(void);
void ex_segment_overrun(void);
void ex_invalid_TSS(void);
void ex_no_segment(void);
void ex_seg_fault(void);
void ex_gen_protection(void);
void ex_page_fault(void);
void ex_reserved(void);
void ex_float_pt_err(void);
void ex_align_check(void);
void ex_machine_check(void);
void ex_SIMD_float_pt(void);

//interrupt handlers
void test_interrupts(void);

/* Writes a byte to a port */
#define outb(data, port)                \
{                                    \
    asm volatile("outb  %b1, (%w0)"     \
            :                           \
            : "d" (port), "a" (data)    \
            : "memory", "cc" );         \
}

/* Writes two bytes to two consecutive ports */
#define outw(data, port)                \
{                                    \
    asm volatile("outw  %w1, (%w0)"     \
            :                           \
            : "d" (port), "a" (data)    \
            : "memory", "cc" );         \
}

/* Writes four bytes to four consecutive ports */
#define outl(data, port)                \
{                                    \
    asm volatile("outl  %l1, (%w0)"     \
            :                           \
            : "d" (port), "a" (data)    \
            : "memory", "cc" );         \
}

/* Clear interrupt flag - disables interrupts on this processor */
#define cli()                           \
{                                    \
    asm volatile("cli"                  \
            :                       \
            :                       \
            : "memory", "cc"        \
            );                      \
}

/* Save flags and then clear interrupt flag
 * Saves the EFLAGS register into the variable "flags", and then
 * disables interrupts on this processor */
#define cli_and_save(flags)             \
{                                    \
    asm volatile("pushfl        \n      \
            popl %0         \n      \
            cli"                    \
            : "=r"(flags)           \
            :                       \
            : "memory", "cc"        \
            );                      \
}

/* Set interrupt flag - enable interrupts on this processor */
#define sti()                           \
{                                    \
    asm volatile("sti"                  \
            :                       \
            :                       \
            : "memory", "cc"        \
            );                      \
}

/* Restore flags
 * Puts the value in "flags" into the EFLAGS register.  Most often used
 * after a cli_and_save_flags(flags) */
#define restore_flags(flags)            \
{                                    \
    asm volatile("pushl %0      \n      \
            popfl"                  \
            :                       \
            : "r"(flags)            \
            : "memory", "cc"        \
            );                      \
}

/* Saves registers */
#define save_regs(retval)\
{                                    \
    asm("" \
            : "=a"(retval.eax), "=b"(retval.ebx), "=c"(retval.ecx), "=d"(retval.edx), "=S"(retval.esi), "=D"(retval.edi) \
            : /* no inputs */);	\
    asm("movl %%esp, %%eax"		\
            : "=a"(retval.esp)	\
            : /* no inputs */);	\
    asm("movl %%ebp, %%eax"		\
            : "=a"(retval.ebp)	\
            : /* no inputs */);	\
    asm("pushf; \n				\
            popl %%eax"				\
            : "=a"(retval.eflags)	\
            : /* no inputs */	\
            :"memory");			\
    asm("xorl %%eax, %%eax" :);	\
    asm("movw %%es, %%ax"		\
            : "=a"(retval.es)	\
            : /* no inputs */);	\
    asm("movw %%cs, %%ax"		\
            : "=a"(retval.cs)	\
            : /* no inputs */);	\
    asm("movw %%ss, %%ax"		\
            : "=a"(retval.ss)	\
            : /* no inputs */);	\
    asm("movw %%ds, %%ax"		\
            : "=a"(retval.ds)	\
            : /* no inputs */);	\
    asm("movw %%fs, %%ax"		\
            : "=a"(retval.fs)	\
            : /* no inputs */);	\
    asm("movw %%gs, %%ax"		\
            : "=a"(retval.gs)	\
            : /* no inputs */);	\
    asm("pushl %% eax; \n				\
            popfl"				\
            : /* no outputs */	\
            : "a"(retval.eflags)	\
            :"memory");			\
    asm("" \
            :  /* no outputs */ \
            : "a"(retval.eax));	\
}

/* Restores registers */
#define restore_regs(retval)\
{                                    \
    asm("xorl %%eax, %%eax" :);	\
    asm("movw %%ax, %%es"		\
            : /* no outputs */	\
            : "a"(retval.es));	\
    asm("movw %%ax, %%ds"		\
            : /* no outputs */	\
            : "a"(retval.ds));	\
    asm("movw %%ax, %%fs"		\
            : /* no outputs */	\
            : "a"(retval.fs));	\
    asm("movw %%ax, %%gs"		\
            : /* no outputs */	\
            : "a"(retval.gs));	\
    asm("movw %%ax, %%ss"		\
            : /* no outputs */	\
            : "a"(retval.ss));	\
    asm("movl %%eax, %%esp"		\
            : /* no outputs */	\
            : "a"(retval.esp));	\
    asm("movl %%eax, %%ebp"		\
            : /* no outputs */	\
            : "a"(retval.ebp));	\
    asm("pushl %% eax; \n				\
            popfl"				\
            : /* no outputs */	\
            : "a"(retval.eflags)	\
            :"memory");			\
    asm("" \
            :  /* no outputs */ \
            : "a"(retval.eax), "b"(retval.ebx), "c"(retval.ecx), "d"(retval.edx), "S"(retval.esi), "D"(retval.edi));	\
}

// restore eax from a structure, so that interrupts do not mess with return
// values of functions they interrupt
#define restore_ret(retval) { \
    asm("" \
            : /* no outputs */ \
            : "a"(retval.eax) \
       ); \
}

// Doesn't mess with EAX, EBP, or ESP.  This is the function that should be used in function calls.
// (we want to use EAX to save a return value and not mess with the stack)
#define restore_regs_in_function(retval)\
{                                    \
    asm("xorl %%ebx, %%ebx" :);	\
    asm("movw %%bx, %%es"		\
            : /* no outputs */	\
            : "b"(retval.es));	\
    asm("movw %%bx, %%ds"		\
            : /* no outputs */	\
            : "b"(retval.ds));	\
    asm("movw %%bx, %%fs"		\
            : /* no outputs */	\
            : "b"(retval.fs));	\
    asm("movw %%bx, %%gs"		\
            : /* no outputs */	\
            : "b"(retval.gs));	\
    asm("movw %%bx, %%ss"		\
            : /* no outputs */	\
            : "b"(retval.ss));	\
    asm("pushl %% ebx;			\
            popfl"					\
            : /* no outputs */	\
            : "b"(retval.eflags)	\
            :"memory");			\
    asm("" \
            :  /* no outputs */ \
            : "b"(retval.ebx), "c"(retval.ecx), "d"(retval.edx), "S"(retval.esi), "D"(retval.edi));	\
}

#endif /* _LIB_H */
