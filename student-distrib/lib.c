/* lib.c - Some basic library functions (printf, strlen, etc.)
 * vim:ts=4:sw=4:et
 */

#include "lib.h"
#include "syscall.h"
#include "keyboard.h"
#include "task.h"

#include "colors.h"

static int screen_x;
static int screen_y;
uint8_t cursor_on = 1;
uint8_t current_attrib = ATTRIB;
extern uint32_t visible_terminal;
extern process_t* current_process;
extern terminal_info_t* current_terminal;

#define MAX_SUBSCRIBERS 3
static attrib_updated_t attrib_updated[MAX_SUBSCRIBERS] = {NULL};

uint32_t vidmem;
uint8_t* real_vidmem = (uint8_t*)VIDEO;

    void
set_screen_coordinates(int x, int y)
{
    while (x < 0) {
        x += NUM_COLS;
        y--;
    }
    while (x >= NUM_COLS) {
        x -= NUM_COLS;
        y++;
    }
    // wraparound (vertical)
    while (y < 0) {
        y += NUM_ROWS;
    }
    
	while(y >= NUM_ROWS) y--;
	
    set_char_attrib(screen_x, screen_y, current_attrib);
    screen_x = x;
    screen_y = y;
    update_cursor();
}

void set_cursor_position(int x, int y)
{
    coord_t old_position = read_screen_coordinates();
    set_char_attrib(old_position.x, old_position.y, current_attrib);

    while (x < 0) {
        x += NUM_COLS;
        y--;
    }
    while (x >= NUM_COLS) {
        x -= NUM_COLS;
        y++;
    }
    // wraparound (vertical)
    while (y < 0) {
        y += NUM_ROWS;
    }
    
	while(y >= NUM_ROWS) y--;
    if (cursor_on)
        set_char_attrib(x, y, CURSOR_ATTRIB);
    else
        set_char_attrib(x,y, current_attrib);
}

void clear_char_attrib(int x, int y)
{
    while (x < 0) {
        x += NUM_COLS;
        y--;
    }
    while (x >= NUM_COLS) {
        x -= NUM_COLS;
        y++;
    }
    // wraparound (vertical)
    while (y < 0) {
        y += NUM_ROWS;
    }
    y %= NUM_ROWS;
    set_char_attrib(x, y, ATTRIB);
}

void set_attrib(uint8_t attrib)
{
    current_attrib = attrib;
}

void add_attrib_observer(attrib_updated_t f) {
    int i;
    for (i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (attrib_updated[i] == NULL) {
            attrib_updated[i] = f;
            return;
        }
    }
}

void set_char_attrib(int x, int y, uint8_t attrib)
{
    int off = y * NUM_COLS + x;
    *(uint8_t *)(real_vidmem + off * 2 + 1) = attrib; 
    int i;
    for (i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (attrib_updated[i] != NULL) {
            attrib_updated[i](x, y);
        }
    }
}

uint8_t get_char_attrib(int x, int y)
{
    int off = y * NUM_COLS + x;
    return *(uint8_t *)(real_vidmem + off * 2 + 1);
}

coord_t read_screen_coordinates(void)
{
    coord_t retval;
    retval.x = screen_x;
    retval.y = screen_y;
    return retval;
}

    void
clear(void)
{
    int32_t blank = (current_attrib << 8) | ' ';
    memset_word(real_vidmem, blank, NUM_ROWS*NUM_COLS);
    set_screen_coordinates(0, 0);
}

void clear_line(int8_t start_x, int8_t line)
{
	int8_t x = start_x;
	int8_t y = line;
    if (x >= NUM_COLS) {
        x = NUM_COLS - 1;
    }
    if (y >= NUM_ROWS) {
        y = NUM_ROWS - 1;
    }
    int16_t blank = (current_attrib << 8) | ' ';
    memset_word((uint8_t*) real_vidmem + ((NUM_COLS*y + x) << 1), blank,
            NUM_COLS - start_x);
}

void scroll(void)
{
    map_backing_page(current_terminal);
    scroll_backing(current_terminal);
	int x, y;
	for(y=0; y < NUM_ROWS - 1; y++) {
		for(x=0; x < NUM_COLS; x++) {
			*(uint8_t *)(real_vidmem + ((NUM_COLS*y + x) << 1)) = *(uint8_t *)(real_vidmem + ((NUM_COLS*(y+1) + x) << 1));
        }
    }
	for(x=0; x < NUM_COLS; x++)
		*(uint8_t *)(real_vidmem + ((NUM_COLS*(NUM_ROWS - 1) + x) << 1)) = 0x00;
}


/* Standard printf().
 * Only supports the following format strings:
 * %%  - print a literal '%' character
 * %x  - print a number in hexadecimal
 * %u  - print a number as an unsigned integer
 * %d  - print a number as a signed integer
 * %c  - print a character
 * %s  - print a string
 * %#x - print a number in 32-bit aligned hexadecimal, i.e.
 *       print 8 hexadecimal digits, zero-padded on the left.
 *       For example, the hex number "E" would be printed as
 *       "0000000E".
 *       Note: This is slightly different than the libc specification
 *       for the "#" modifier (this implementation doesn't add a "0x" at
 *       the beginning), but I think it's more flexible this way.
 *       Also note: %x is the only conversion specifier that can use
 *       the "#" modifier to alter output.
 * */
    int32_t
printf(int8_t *format, ...)
{
    /* Pointer to the format string */
    int8_t* buf = format;

    /* Stack pointer for the other parameters */
    int32_t* esp = (void *)&format;
    esp++;

    while(*buf != '\0') {
        switch(*buf) {
            case '%':
                {
                    int32_t alternate = 0;
                    buf++;

format_char_switch:
                    /* Conversion specifiers */
                    switch(*buf) {
                        /* Print a literal '%' character */
                        case '%':
                            putc('%');
                            break;

                            /* Use alternate formatting */
                        case '#':
                            alternate = 1;
                            buf++;
                            /* Yes, I know gotos are bad.  This is the
                             * most elegant and general way to do this,
                             * IMHO. */
                            goto format_char_switch;

                            /* Print a number in hexadecimal form */
                        case 'x':
                            {
                                int8_t conv_buf[64];
                                if(alternate == 0) {
                                    itoa(*((uint32_t *)esp), conv_buf, 16);
                                    puts(conv_buf);
                                } else {
                                    int32_t starting_index;
                                    int32_t i;
                                    itoa(*((uint32_t *)esp), &conv_buf[8], 16);
                                    i = starting_index = strlen(&conv_buf[8]);
                                    while(i < 8) {
                                        conv_buf[i] = '0';
                                        i++;
                                    }
                                    puts(&conv_buf[starting_index]);
                                }
                                esp++;
                            }
                            break;

                            /* Print a number in unsigned int form */
                        case 'u':
                            {
                                int8_t conv_buf[36];
                                itoa(*((uint32_t *)esp), conv_buf, 10);
                                puts(conv_buf);
                                esp++;
                            }
                            break;

                            /* Print a number in signed int form */
                        case 'd':
                            {
                                int8_t conv_buf[36];
                                int32_t value = *((int32_t *)esp);
                                if(value < 0) {
                                    conv_buf[0] = '-';
                                    itoa(-value, &conv_buf[1], 10);
                                } else {
                                    itoa(value, conv_buf, 10);
                                }
                                puts(conv_buf);
                                esp++;
                            }
                            break;

                            /* Print a single character */
                        case 'c':
                            putc( (uint8_t) *((int32_t *)esp) );
                            esp++;
                            break;

                            /* Print a NULL-terminated string */
                        case 's':
                            puts( *((int8_t **)esp) );
                            esp++;
                            break;

                        default:
                            break;
                    }

                }
                break;

            default:
                putc(*buf);
                break;
        }
        buf++;
    }

    return (buf - format);
}

/* Output a string to the console */
    int32_t
puts(int8_t* s)
{
    register int32_t index = 0;
    while(s[index] != '\0') {
        putc(s[index]);
        index++;
    }

    return index;
}

    int32_t
puts_wrap(int8_t* s)
{
	coord_t current; 
    register int32_t index = 0;
    while(s[index] != '\0') {
        current = read_screen_coordinates();
        putc(s[index]);
        index++;
/*		
        if(current.x == 79)
        {
            set_screen_coordinates(0, current.y + 1);
        }
*/		
    }
    return index;
}

    void
putc(uint8_t c)
{
    int x = screen_x;
    int y = screen_y;
//    uint8_t *x = &terminals[current_terminal->index].current_position.x;
//    uint8_t *y = &terminals[current_terminal->index].current_position.y;

    if(c == '\n' || c == '\r') {
        set_char_attrib(x, y, current_attrib);
        y += 1;
//        y++;

		if(y >= NUM_ROWS)
		{
			scroll();
			y -= 1;
		}
        x=0;
    }
	else 
	{
		*(uint8_t *)(real_vidmem + ((NUM_COLS* y + x) << 1)) = c;
        *(uint8_t *)(real_vidmem + ((NUM_COLS* y + x) << 1) + 1) =
            current_attrib;
        x++;
    }
	if(x == NUM_COLS && y == NUM_ROWS - 1)
	{
		scroll();
		x=0;
		current_terminal->keyboard_start_coord.y--;
	}
    set_screen_coordinates(x,y);
    set_char_attrib(x, y, current_attrib);
}


void update_cursor(void)
{
    set_cursor_position(screen_x, screen_y);
}

void increment_vid_mem(void)
{
    int32_t i;
    for (i=0; i < NUM_ROWS*NUM_COLS; i++) {
        real_vidmem[i<<1]++;
    }
}

void increment_video_location(int col, int row)
{
    int off = (row * NUM_COLS + col) * 2;
    real_vidmem[off]++;
}

/* Convert a number to its ASCII representation, with base "radix" */
    int8_t*
itoa(uint32_t value, int8_t* buf, int32_t radix)
{
    static int8_t lookup[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    int8_t *newbuf = buf;
    int32_t i;
    uint32_t newval = value;

    /* Special case for zero */
    if(value == 0) {
        buf[0]='0';
        buf[1]='\0';
        return buf;
    }

    /* Go through the number one place value at a time, and add the
     * correct digit to "newbuf".  We actually add characters to the
     * ASCII string from lowest place value to highest, which is the
     * opposite of how the number should be printed.  We'll reverse the
     * characters later. */
    while(newval > 0) {
        i = newval % radix;
        *newbuf = lookup[i];
        newbuf++;
        newval /= radix;
    }

    /* Add a terminating NULL */
    *newbuf = '\0';

    /* Reverse the string and return */
    return strrev(buf);
}

/* In-place string reversal */
    int8_t*
strrev(int8_t* s)
{
    register int8_t tmp;
    register int32_t beg=0;
    register int32_t end=strlen(s) - 1;

    while(beg < end) {
        tmp = s[end];
        s[end] = s[beg];
        s[beg] = tmp;
        beg++;
        end--;
    }

    return s;
}

/* String length */
    uint32_t
strlen(const int8_t* s)
{
    register uint32_t len = 0;
    while(s[len] != '\0')
        len++;

    return len;
}

// appends src to dest, where dest has size bytes allocated
// returns the new size of dest
uint32_t strlcat(int8_t *dest, const int8_t *src, uint32_t size) {
    int i;
    for (i = 0; i < size; i++) {
        if (dest[i] == '\0') {
            break;
        }
    }
    for (; i < size; src++, i++) {
        if (*src != '\0') {
            dest[i] = *src;
        } else {
            break;
        }
    }
    if (i < size) {
        dest[i] = '\0';
    }

    return i;
}

/* Optimized memset */
    void*
memset(void* s, int32_t c, uint32_t n)
{
    c &= 0xFF;
    asm volatile ("             \n\
        .memset_top:            \n\
        testl   %%ecx, %%ecx    \n\
        jz      .memset_done    \n\
        testl   $0x3, %%edi     \n\
        jz      .memset_aligned \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        subl    $1, %%ecx       \n\
        jmp     .memset_top     \n\
        .memset_aligned:        \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        movl    %%ecx, %%edx    \n\
        shrl    $2, %%ecx       \n\
        andl    $0x3, %%edx     \n\
        cld                     \n\
        rep     stosl           \n\
        .memset_bottom:         \n\
        testl   %%edx, %%edx    \n\
        jz      .memset_done    \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        subl    $1, %%edx       \n\
        jmp     .memset_bottom  \n\
        .memset_done:           \n\
        "
        :
        : "a"(c << 24 | c << 16 | c << 8 | c), "D"(s), "c"(n)
        : "edx", "memory", "cc" );

    return s;
}

/* Optimized memset_word */
    void*
memset_word(void* s, int32_t c, uint32_t n)
{
    asm volatile ("             \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        cld                     \n\
        rep     stosw           \n\
        "
        :
        : "a"(c), "D"(s), "c"(n)
        : "edx", "memory", "cc" );

    return s;
}

/* Optimized memset_dword */
    void*
memset_dword(void* s, int32_t c, uint32_t n)
{
    asm volatile ("             \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        cld                     \n\
        rep     stosl           \n\
        "
        :
        : "a"(c), "D"(s), "c"(n)
        : "edx", "memory", "cc" );

    return s;
}

/* Optimized memcpy */
    void*
memcpy(void* dest, const void* src, uint32_t n)
{
    asm volatile ("             \n\
        .memcpy_top:            \n\
        testl   %%ecx, %%ecx    \n\
        jz      .memcpy_done    \n\
        testl   $0x3, %%edi     \n\
        jz      .memcpy_aligned \n\
        movb    (%%esi), %%al   \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        addl    $1, %%esi       \n\
        subl    $1, %%ecx       \n\
        jmp     .memcpy_top     \n\
        .memcpy_aligned:        \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        movl    %%ecx, %%edx    \n\
        shrl    $2, %%ecx       \n\
        andl    $0x3, %%edx     \n\
        cld                     \n\
        rep     movsl           \n\
        .memcpy_bottom:         \n\
        testl   %%edx, %%edx    \n\
        jz      .memcpy_done    \n\
        movb    (%%esi), %%al   \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        addl    $1, %%esi       \n\
        subl    $1, %%edx       \n\
        jmp     .memcpy_bottom  \n\
        .memcpy_done:           \n\
        "
        :
        : "S"(src), "D"(dest), "c"(n)
        : "eax", "edx", "memory", "cc" );

    return dest;
}

/* Optimized memmove (used for overlapping memory areas) */
    void*
memmove(void* dest, const void* src, uint32_t n)
{
    asm volatile ("             \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        cld                     \n\
        cmp     %%edi, %%esi    \n\
        jae     .memmove_go     \n\
        leal    -1(%%esi, %%ecx), %%esi    \n\
        leal    -1(%%edi, %%ecx), %%edi    \n\
        std                     \n\
        .memmove_go:            \n\
        rep     movsb           \n\
        "
        :
        : "D"(dest), "S"(src), "c"(n)
        : "edx", "memory", "cc" );

    return dest;
}

/* Standard strncmp */
    int32_t
strncmp(const int8_t* s1, const int8_t* s2, uint32_t n)
{
    int32_t i;
    for(i=0; i<n; i++) {
        if( (s1[i] != s2[i]) ||
                (s1[i] == '\0') /* || s2[i] == '\0' */ ) {

            /* The s2[i] == '\0' is unnecessary because of the short-circuit
             * semantics of 'if' expressions in C.  If the first expression
             * (s1[i] != s2[i]) evaluates to false, that is, if s1[i] ==
             * s2[i], then we only need to test either s1[i] or s2[i] for
             * '\0', since we know they are equal. */

            return s1[i] - s2[i];
        }
    }
    return 0;
}

//compares two strings, returns position of where two strings differ
//returns -1 if identical
int32_t strcmp(const char * s1, const char * s2)
{
	int n1 = strlen(s1);
	int n2 = strlen(s2);
	int n = (n1 >= n2) ? n1 : n2;
	int i;
	for(i=0; i < n; i++)
		if(s1[i] != s2[i]) return i;

	return -1;
}

//returns 1 if s1 is contained within s2, else -1
int32_t substr(const char * s1, const char * s2)
{
	int i;
	int n = strlen(s1);
	for(i=0; i < n; i++)
		if(s1[i] != s2[i]) return -1;
	return 1;
}

/* Standard strcpy */
    int8_t*
strcpy(int8_t* dest, const int8_t* src)
{
    int32_t i=0;
    while(src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
    return dest;
}

/* Standard strncpy */
    int8_t*
strncpy(int8_t* dest, const int8_t* src, uint32_t n)
{
    int32_t i=0;
    while(src[i] != '\0' && i < n) {
        dest[i] = src[i];
        i++;
    }

    while(i < n) {
        dest[i] = '\0';
        i++;
    }

    return dest;
}

//general interrupt handler
    void
test_interrupts(void)
{
    //	int32_t i;
    //	for (i=0; i < NUM_ROWS*NUM_COLS; i++) {
    //		real_vidmem[i<<1]++;
    //	}
    //	printf("Something happened!\n");
    //	for(i = 0; i < 16; i++)
    //	{
    //	  send_eoi(i);
    //	}
    asm volatile("      \
        leave;          \
        iret"
        :
        :
        :"memory" );
}

void init_graphics()
{
    /* Disable hardware cursor */
    unsigned char csr = inb(0x3CC);
    csr |= 0x10;
    outb(0xA, 0x3D4);
    outb(csr, 0x3D5);
}

void bsod(void)
{
    set_attrib(BACK(BLUE) | FORE(BRIGHT(GRAY)));
    clear();    
	printSB();
}

void shutdown_screen(void)
{
	set_attrib(BACK(BLACK) | FORE(RED));
	clear();
	printf("Shutting Down...\n\n");
	printXiOS();
}
//below are all exception handlers

void startup_screen(void)
{
	set_attrib(BACK(BLACK) | FORE(RED));
	clear();
	printf("\n\n");
	printXiOS();
}

void ex_divide_error(void)
{
    clear();
    printf("EXCEPTION 0: Divide-by-Zero Error, or infinity... not sure which\n");
    syscall_halt(-1);
}
void ex_debug(void)
{
    clear();
    printf("EXCEPTION 1: Debug\n");
    syscall_halt(-1);
}
void ex_NMI(void)
{
    clear();
    printf("EXCEPTION 2: NMI Interrupt\n");
    syscall_halt(-1);
}
void ex_breakpoint(void)
{
    clear();
    printf("EXCEPTION 3: Breakpoint\n");
    syscall_halt(-1);
}
void ex_overflow(void)
{
    clear();
    printf("EXCEPTION 4: Overflow\n");
    syscall_halt(-1);
}
void ex_bound_range(void)
{
    clear();
    printf("EXCEPTION 5: BOUND Range Exceeded\n");
    syscall_halt(-1);
}
void ex_invalid_op(void)
{
    clear();
    printf("EXCEPTION 6: Invalid Opcode\n");
    syscall_halt(-1);
}
void ex_device_na(void)
{
    clear();
    printf("EXCEPTION 7: Device Not Available\n");
    syscall_halt(-1);
}
void ex_double_fault(void)
{
    clear();
    printf("EXCEPTION 8: Double Fault\n");
    syscall_halt(-1);
}
void ex_segment_overrun(void)
{
    clear();
    printf("EXCEPTION 9: CoProcessor Segment Overrun\n");
    syscall_halt(-1);
}
void ex_invalid_TSS(void)
{
    clear();
    printf("EXCEPTION 10: Invalid TSS\n");
    syscall_halt(-1);
}
void ex_no_segment(void)
{
    clear();
    printf("EXCEPTION 11: Segment Not Present\n");
    syscall_halt(-1);
}
void ex_seg_fault(void)
{
    clear();
    printf("EXCEPTION 12: Stack Segment Fault\n");
    syscall_halt(-1);
}
void ex_gen_protection(void)
{
    clear();
    printf("EXCEPTION 13: General Protection Fault\n");
    syscall_halt(-1);
}
void ex_page_fault(void)
{
    //clear();
    //bsod();
    uint32_t paged_mem;
    //copy memory that caused ex into paged_mem;
    asm volatile ("         \
        movl %%CR2, %[addr]"
        :[addr]"=r"(paged_mem) /*output*/ );
    printf("EXCEPTION 14: Page Fault\nAttempted to Access Memory at: 0x%#x\n",paged_mem);
    syscall_halt(-1);
}
void ex_reserved(void)
{
    clear();
    printf("EXCEPTION: Reserved\n");
    syscall_halt(-1);
}
void ex_float_pt_err(void)
{
    clear();
    printf("EXCEPTION 16: Floating Point Error\n");
    syscall_halt(-1);
}
void ex_align_check(void)
{
    clear();
    printf("EXCEPTION 17: Alignment Check\n");
    syscall_halt(-1);
}
void ex_machine_check(void)
{
    clear();
    printf("EXCEPTION 18: Machine Check\n");
    syscall_halt(-1);
}
void ex_SIMD_float_pt(void)
{
    clear();
    printf("EXCEPTION 19: SIMD Floating-Point\n");
    syscall_halt(-1);
}

void printSB(void)
{
    char sb[21][36] = {
        "              _______              \0",
        "            .'.     .'.            \0",
        "           / .|\\ _ /|. \\           \0",
        "          : :  \\\\ //  : :          \0",
        "          : : (_\\V/_) : :          \0",
        "          : :    v    : :          \0",
        "          :  \\ .---. /  :          \0",
        "           \\  \\|   |/  /           \0",
        "            \\  |===|  /            \0",
        "  .-.__      '..___..'      __.-.  \0",
        " /  /  |       _| |_       |  \\  \\ \0",
        "|   |/ |'|----/     \\----|'| \\|   |\0",
        "|      |.|---:       :---|.|      |\0",
        " \\    _|     :_______:     |_    / \0",
        "  '--'        #######        '--'  \0",
        "              '#####'              \0",
        "              /#\"\"\"#\\              \0",
        "             /#/   \\#\\             \0",
        "         .-./\\/     \\/\\.-.         \0",
        "        \\    /       \\    /        \0",
        "         '._/         \\_.'         \0"
    };
    int i;
    for(i=0; i < 21; i++)
    {
        if(i==0)      printf("\nFRAGRANT SYSTEM ERROR!");
        else if(i==1) printf("Computer Over.        ");
        else if(i==2) printf("VIRUS = Very Yes.     ");
        else         printf("                      ");	
        printf("%s\n",sb[i]);
    }
}

void abort(char* exp, char* file, int line) {
    printf("\nfailure of %s at file %s +%d\n", exp, file, line);
    asm(" \
            cli; \
            abort_loop: \
            jmp abort_loop; \
            ");
}

void printXiOS(void)
{
	char xios[16][70] ={
"XXXXXXX       XXXXXXX   iiii        OOOOOOOOO        SSSSSSSSSSSSSSS ",
"X:::::X       X:::::X  i::::i     OO:::::::::OO    SS:::::::::::::::S",
"X:::::X       X:::::X   iiii    OO:::::::::::::OO S:::::SSSSSS::::::S",
"X::::::X     X::::::X          O:::::::OOO:::::::OS:::::S     SSSSSSS",
"XXX:::::X   X:::::XXX iiiiiii  O::::::O   O::::::OS:::::S            ",
"   X:::::X X:::::X    i:::::i  O:::::O     O:::::OS:::::S            ",
"    X:::::X:::::X      i::::i  O:::::O     O:::::O S::::SSSS         ",
"     X:::::::::X       i::::i  O:::::O     O:::::O  SS::::::SSSSS    ",
"     X:::::::::X       i::::i  O:::::O     O:::::O    SSS::::::::SS  ",
"    X:::::X:::::X      i::::i  O:::::O     O:::::O       SSSSSS::::S ",
"   X:::::X X:::::X     i::::i  O:::::O     O:::::O            S:::::S",
"XXX:::::X   X:::::XXX  i::::i  O::::::O   O::::::O            S:::::S",
"X::::::X     X::::::X i::::::i O:::::::OOO:::::::OSSSSSSS     S:::::S",
"X:::::X       X:::::X i::::::i  OO:::::::::::::OO S::::::SSSSSS:::::S",
"X:::::X       X:::::X i::::::i    OO:::::::::OO   S:::::::::::::::SS ",
"XXXXXXX       XXXXXXX iiiiiiii      OOOOOOOOO      SSSSSSSSSSSSSSS   "
};
	int i;
	for(i=0; i < 16; i++)
		printf("     %s\n",xios[i]);
	printf("\n        Matthew Tischer | Tej Chajed | Hanz Anderson | Matthew Johnson\n");
}
