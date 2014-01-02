// vim: ts=4:sw=4:et
/**
 * keyboard.c
 *
 * Implementation of the terminal driver
 */
#include "keyboard.h"
#include "mem.h"
#include "paging.h"
#include "spinlock.h"
#include "lib.h"
#include "status.h"
#include "mouse.h"

// temporary, until common interrupt handling is separated
#include "i8259.h"

// define's for scancodes
// modifiers
#define L_CTRL_KEY 0x1D
#define L_ALT_KEY 0x38
#define L_SHIFT_KEY 0x2A
#define R_SHIFT_KEY 0x36
#define CAPS_LOCK_KEY 0x3A

// arrow keys
#define LEFT_ARROW_KEY 0x4B
#define RIGHT_ARROW_KEY 0x4D
#define UP_ARROW_KEY 0x48
#define DOWN_ARROW_KEY 0x50
#define PGUP_KEY 0x49
#define PGDOWN_KEY 0x51

// special keys
#define ENTER_KEY 0x1C
#define BACKSPACE_KEY 0x0E
#define DELETE_KEY 0x53
#define SPACE_KEY 0x39
#define TAB_KEY 0x0F
#define ESC_KEY 0x01

// some function keys
#define F_KEY(n) (0x3A + n)

// letters that have special functions with Ctrl
#define L_KEY 0x26
#define A_KEY 0x1E
#define K_KEY 0x25

// get the release code for a key
#define RELEASE(key) (key|0x80)

//
#define MAX_SCROLLBACK_OFFSET 5

// Arrays to hold the list of printable keyboard characters.
static uint8_t keyboard_char[64] = {
    '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\0', '\0',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\0', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' ', '\0', '\0', '\0', '\0', '\0', '\0'};

static uint8_t keyboard_char_shift[64] = {
    '\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\0', '\0',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\0', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' ', '\0', '\0', '\0', '\0', '\0', '\0'};

//	
#define MAX_EXECUTABLES 15
static char *dir[MAX_EXECUTABLES];

// Global flags for modifier keys.
static uint8_t keyboard_shift_set = 0;
static uint8_t keyboard_ctrl_set = 0;
static uint8_t keyboard_alt_set = 0;

// Spinlock for synchronization.
static spinlock_t terminal_lock = SPINLOCK_UNLOCKED;
//#define LOCK() spin_lock(&terminal_lock); printf("locked at %d\n", __LINE__)
//#define UNLOCK() spin_unlock(&terminal_lock); printf("unlocked at %d\n", __LINE__)
#define LOCK() spin_lock(&terminal_lock)
#define UNLOCK() spin_unlock(&terminal_lock)

// Information about the system's terminals.
terminal_info_t* terminals;

// Keeps track of the process currently running in each terminal.
process_t* process_in_terminal[NUM_TERMINALS];

// Keeps track of the currently-visible terminal itself.
terminal_info_t* current_terminal;

// Forward declarations.
static void clear_buffer(void *buffer, uint32_t size);

// External declarations.
extern uint8_t cursor_on;
extern uint8_t current_attrib;

// Scrollback offset of the current page
static int32_t scrollback_offset = 0;

/**
 * Function to clear an arbitrary buffer.
 * @param buffer The buffer to clear.
 * @param size The amount of the buffer to clear (in bytes).
 */
void clear_buffer(void *buffer, uint32_t size) {
    memset(buffer, 0, size);
}

/**
 * The keypress interrupt handler.
 */
void keypress_handler(void)
{
    int32_t i;

    uint8_t scan[3];

    //save all
    registers_t regs;
    save_regs(regs);

    LOCK();
	// Read bytes from the keyboard.
    for(i = 0; i < 3; i++)
    {
        scan[i] = inb(0x60);
    }
    if (scan[0] == 0xE0) {
        scan[0] = scan[1];
    }
    // Check for special scan codes.
    if(scan[0] != 0xE0)
    {
        cursor_on = 1;
        coord_t current = read_screen_coordinates();
		
		// handle shift
        if(scan[0] == L_SHIFT_KEY || scan[0] == R_SHIFT_KEY)
        {
            keyboard_shift_set = 1;
        }
		
		// handle releasing shift
        else if(scan[0] == RELEASE(L_SHIFT_KEY) || scan[0] == RELEASE(R_SHIFT_KEY))
        {
            keyboard_shift_set = 0;
        }
		
		// handle ctrl
        else if(scan[0] == L_CTRL_KEY)
        {
            keyboard_ctrl_set = 1;
        }
		
		// handle releasing ctrl
        else if(scan[0] == RELEASE(L_CTRL_KEY))
        {
            keyboard_ctrl_set = 0;
        }
		
		// handle alt
        else if(scan[0] == L_ALT_KEY)
        {
            keyboard_alt_set = 1;
        }
		
		// handle releasing alt
        else if(scan[0] == RELEASE(L_ALT_KEY))
        {
            keyboard_alt_set = 0;
        }
		
		// handle left arrow key
        else if(scan[0] == LEFT_ARROW_KEY)
        {
            // Left arrow key has been pressed.  Move the current location to
            // the left, if we're not at the start of the string.
            coord_t current = read_screen_coordinates();
            if(current_terminal->keyboard_buffer_pos > 0)
            {
                // Clear the current cursor image.
                clear_char_attrib(current_terminal->keyboard_start_coord.x +
                        current_terminal->keyboard_buffer_pos,
                        current_terminal->keyboard_start_coord.y);

				// We've moved to the left in the buffer.
                current_terminal->keyboard_buffer_pos--;

				// Actually move the cursor.
                set_screen_coordinates(current.x - 1, current.y);
            }
        }
		
		// handle right arrow key
        else if(scan[0] == RIGHT_ARROW_KEY)
        {
            // Right arrow key has been pressed.  Move the current location to 
			// the right, if we're not at the end of the string.
            coord_t current = read_screen_coordinates();
            if(current_terminal->keyboard_buffer_pos < current_terminal->keyboard_buffer_size)
            {
                // Clear the current cursor image.
                clear_char_attrib(current_terminal->keyboard_start_coord.x +
                        current_terminal->keyboard_buffer_pos,
                        current_terminal->keyboard_start_coord.y);
				
				// We've moved to the right in the buffer.
                current_terminal->keyboard_buffer_pos++;

				// Actually move the cursor.
                set_screen_coordinates(current.x + 1, current.y);
            }
        }
		
		// handle up arror key
		else if(scan[0] == UP_ARROW_KEY)
		{
            scrollback_offset = 0;
            set_scrollback_page(0);
            history_move(-1);
		}
		
		// handle down arrow key
		else if(scan[0] == DOWN_ARROW_KEY)
		{
            scrollback_offset = 0;
            set_scrollback_page(0);
            history_move(1);
        }
		
        else if(scan[0] == PGUP_KEY)
        {
            adjust_scrollback_page(1);
        }

        else if(scan[0] == PGDOWN_KEY)
        {
          adjust_scrollback_page(-1);            
        }

	    // handle alt + f-key (switch terminals)
        else if(keyboard_alt_set && (scan[0] >= F_KEY(1)) && (scan[0] <= F_KEY(7)))
        {
            int f_number = scan[0] - F_KEY(1);
			
            scrollback_offset = 0;
            set_scrollback_page(0);

            UNLOCK();
            switch_terminals(&terminals[f_number]);
            LOCK();
        }	
		
        else if(scan[0] <= SPACE_KEY)
        {
            if (process_in_terminal[current_terminal->index] == NULL) {
                goto exit_handler;
            }
            // Reset scrollback.
            scrollback_offset = 0;
            set_scrollback_page(0);

            if(current_terminal->keyboard_buffer_size == 0)
            {
                // We're writing the first character, so we should declare the
                // start of our string to be here.
                current_terminal->keyboard_start_coord = current;
            }
            if(keyboard_shift_set == 0)
            {
				// handle special keys
                if(scan[0] == BACKSPACE_KEY)
                {
                    handle_backspace();
                }
                else if(scan[0] == ENTER_KEY)
                {
                    handle_enter();
                }
				else if(scan[0] == TAB_KEY)
				{
					handle_tab();
				}

				// Handle "normal" characters.
                else if (!keyboard_ctrl_set && !keyboard_alt_set && (current_terminal->keyboard_buffer_size + 1 < BUFFER_SIZE - 1)) {
                    // Update the keyboard buffer.
					if(keyboard_char[scan[0]] != '\0')
					{
						for(i = current_terminal->keyboard_buffer_size; i > current_terminal->keyboard_buffer_pos; i--)
						{
							current_terminal->keyboard_buffer[i] = current_terminal->keyboard_buffer[i - 1];
						}
						current_terminal->keyboard_buffer[current_terminal->keyboard_buffer_pos] = keyboard_char[scan[0]];
						current_terminal->keyboard_buffer_pos++;
						current_terminal->keyboard_buffer_size++;

						// Reprint the keyboard buffer.
						reprint_keyboard_buffer();

						// Move the keyboard coordinates to where they should be.
						if(current.x < (NUM_COLS - 1))
						{
							set_screen_coordinates(current.x + 1, current.y);
						}
						else
						{
							set_screen_coordinates(0, current.y + 1);
						}
					}
                }
                else
                {
                    // Ctrl-L ("clear" screen)
                    if (keyboard_ctrl_set && scan[0] == L_KEY)
                    {
						coord_t coords = read_screen_coordinates();
						set_screen_coordinates(coords.x, 0);
						current_terminal->keyboard_start_coord.y = 0;
						
						// scroll the screen until the current line is at the top.
						int i;
						for(i=0; i < current.y; i++) {
							scroll();
                        }
                    }
					
                    // Ctrl-A (go to beginning of line)
                    if (keyboard_ctrl_set && scan[0] == A_KEY)
                    {
                        update_cursor();
                        set_screen_coordinates(current_terminal->keyboard_start_coord.x,
                                current_terminal->keyboard_start_coord.y);
                        current_terminal->keyboard_buffer_pos = 0;
                    }
					
                    // Ctrl-K (clear to end of line)
                    if (keyboard_ctrl_set && scan[0] == K_KEY)
                    {
                        coord_t original_coord = read_screen_coordinates();
                        set_screen_coordinates(original_coord.x, original_coord.y);
                        int i;
						
						// Update the buffer.
                        for (i = current_terminal->keyboard_buffer_pos; i <=
                                current_terminal->keyboard_buffer_size; i++)
                        {
                            current_terminal->keyboard_buffer[i] = '\0';
                            putc(' ');
                        }
                        current_terminal->keyboard_buffer_size = current_terminal->keyboard_buffer_pos;
                        // Restore coordinates
                        set_screen_coordinates(original_coord.x, original_coord.y);
                    }
                } 	
            }
            else
            {
				// handle special keys
                if(scan[0] == BACKSPACE_KEY)
                {
                    handle_backspace();
                }
                else if(scan[0] == ENTER_KEY)
                {
                    handle_enter();
                }
				else if(scan[0] == TAB_KEY)
				{
					handle_tab();				
				}
				
				// Handle "normal" characters.
                else if(current_terminal->keyboard_buffer_size + 1 < BUFFER_SIZE - 1)
                {
					if(keyboard_char_shift[scan[0]] != '\0')
					{
						// Update the keyboard buffer.
						for(i = current_terminal->keyboard_buffer_size; i > current_terminal->keyboard_buffer_pos; i--)
						{
							current_terminal->keyboard_buffer[i] = current_terminal->keyboard_buffer[i - 1];
						}
						current_terminal->keyboard_buffer[current_terminal->keyboard_buffer_pos] = keyboard_char_shift[scan[0]];
						current_terminal->keyboard_buffer_pos++;
						current_terminal->keyboard_buffer_size++;

						// Reprint the keyboard buffer.
						reprint_keyboard_buffer();

						// Move the keyboard coordinates to where they were.
						if(current.x < (NUM_COLS - 1))
						{
							set_screen_coordinates(current.x + 1, current.y);
						}
						else
						{
							set_screen_coordinates(0, current.y + 1);
						}
					}
                }
            }
        }
    }

exit_handler:
	
	// send an eoi, because we've handled the interrupt.
    send_eoi(1);
    UNLOCK();

    //restore all
    restore_regs(regs);

    asm volatile ("     \
        leave         \n\
        iret"
        :
        :
        :"memory" );
}

/**
 * Resets the all of the information for the current terminal
 * and places the cursor at (0,0). 
 */
void reset_keyboard(void) {
    int i;
    current_terminal->keyboard_start_coord.x = 0;
    current_terminal->keyboard_start_coord.y = 0;
    current_terminal->keyboard_buffer_pos = 0;
    for (i=0; i < current_terminal->keyboard_buffer_size; i++) {
        current_terminal->keyboard_buffer[i] = '\0';
    }
    current_terminal->keyboard_buffer_size = 0;
    set_screen_coordinates(0,0);
}

/**
 * Handles backspace-related actions.
 */
void handle_backspace(void) {
    // Backspace!  Remove a character from the buffer, if we're
    // not at the beginning.
    if(current_terminal->keyboard_buffer_pos > 0)
    {
        int i;
        coord_t old_coords = read_screen_coordinates();

		// Clear the old cursor image.
        clear_char_attrib(current_terminal->keyboard_start_coord.x + current_terminal->keyboard_buffer_pos, current_terminal->keyboard_start_coord.y);

		// Edit the buffer.
        current_terminal->keyboard_buffer_size--;
        current_terminal->keyboard_buffer_pos--;  
        for(i = current_terminal->keyboard_buffer_pos; i <= current_terminal->keyboard_buffer_size + 1; i++)
        {
            current_terminal->keyboard_buffer[i] = current_terminal->keyboard_buffer[i + 1];
        }

		// Reprint the keyboard buffer, and overwrite the old character.
        reprint_keyboard_buffer();
        putc(' ');

		// Reset the position appropriately.
        if(old_coords.x > 0)
        {
            set_screen_coordinates(old_coords.x - 1, old_coords.y);
        }
        else
        {
            set_screen_coordinates((NUM_COLS - 1), old_coords.y - 1);
        }
    }
}

/**
 * Handles enter-related actions.
 */
void handle_enter(void) {

    int i;	 

    uint32_t *history_size = &current_terminal->history_size;
	
	// Copy keyboard buffer to command history
	if(*history_size == MAX_HISTORY_CMDS)
	{
		int i;
		for(i=0; i < MAX_HISTORY_CMDS - 1; i++) {
            strncpy(current_terminal->command_history[i],
                    current_terminal->command_history[i+1], BUFFER_SIZE);	
        }
		*history_size -= 1;
	}
    strncpy(current_terminal->command_history[*history_size],
            current_terminal->keyboard_buffer, BUFFER_SIZE);
	*history_size += 1;
    current_terminal->history_curr = *history_size; // set to top of history
	
    // Add a newline to the very end of the buffer.
    current_terminal->keyboard_buffer[current_terminal->keyboard_buffer_size] = '\n';

    reprint_keyboard_buffer();

    coord_t current = read_screen_coordinates();
    // Remove trailing whitespace
	i = current_terminal->keyboard_buffer_size - 1;
	while(current_terminal->keyboard_buffer[i] == ' ' && i >= 0)
	{
		current_terminal->keyboard_buffer[i] = '\0';
		i--;	
	}		
	
    current_terminal->keyboard_start_coord = current;

    // Copy the keyboard buffer into the read buffer.
    strncpy(current_terminal->keyboard_read_buffer,
            current_terminal->keyboard_buffer, BUFFER_SIZE);

	
    // Clear the buffer.
    for(i = 0; i < BUFFER_SIZE; i++)
    {
        current_terminal->keyboard_buffer[i] = '\0';
    }
    current_terminal->keyboard_buffer_pos = 0;
    current_terminal->keyboard_buffer_size = 0;

    current_terminal->keyboard_read_flag = 1;
}

/**
 * Changes the current line to the one that is offset positions away in the
 * command history.
 * @param offset Offset to move to (relative to current position) in command history.
 */
void history_move(int32_t offset) {
    int32_t history_pos = current_terminal->history_curr + offset;
    if(0 <= history_pos && history_pos <= current_terminal->history_size)
    {
		// Clear the keyboard buffer.
        clear_buffer(current_terminal->keyboard_buffer, BUFFER_SIZE);

        // Clear the buffer on the screen.
        set_screen_coordinates(current_terminal->keyboard_start_coord.x, current_terminal->keyboard_start_coord.y);
        int i;
        for(i=0; i < current_terminal->keyboard_buffer_size; i++) putc(' ');
		
		// Copy in the new string to the keyboard buffer.
        strncpy(current_terminal->keyboard_buffer,
                current_terminal->command_history[history_pos],
                BUFFER_SIZE);
        current_terminal->keyboard_buffer_pos =
            current_terminal->keyboard_buffer_size =
            strlen(current_terminal->keyboard_buffer);

        // Reprint the buffer on the screen.        
        reprint_keyboard_buffer();

		// Update the current history value.
        current_terminal->history_curr = history_pos;
    }
}

/**
 * Handles tab-related funtions.
 */
void handle_tab(void)
{
	if(current_terminal->keyboard_buffer_size == 0) return;
	
	//Clear line.
	set_screen_coordinates(current_terminal->keyboard_start_coord.x, current_terminal->keyboard_start_coord.y);
	int i;
	for(i=0; i < current_terminal->keyboard_buffer_size; i++) putc(' ');
		
    //Complete command based on available entries.
	tab_complete(); 
	
	// Reprint the keyboard buffer.
    reprint_keyboard_buffer();

    // Note that we are *always* at the end of the keyboard buffer after this operation (command found or not).
    // We want to change the coordinates back to what they should be.
    set_screen_coordinates(current_terminal->keyboard_start_coord.x + current_terminal->keyboard_buffer_pos, 
        current_terminal->keyboard_start_coord.y);

}

/**
 * Tab completion function.
 * Edits the buffer based on current input and files in the filesystem.
 */
void tab_complete(void)
{
	int i, j, len;
	char cmd[BUFFER_SIZE] = ""; //holds the completed cmd
	char buffer[BUFFER_SIZE] = ""; //holds what user input

    int32_t num_files = get_executables(dir, MAX_EXECUTABLES);
	
	//grab the text before that last space
	i = current_terminal->keyboard_buffer_size - 1;
	while(current_terminal->keyboard_buffer[i] != ' ' && i >= 0){i--;}
	
	i++;
	for(j=0; i < current_terminal->keyboard_buffer_size; j++)
	{
		buffer[j] = current_terminal->keyboard_buffer[i];
		i++;
	}
	
	//autocomplete the text from dir entries
	for(i=0; i < num_files; i++)
	{
		//if complete text is a substring of an entry
		if(substr(buffer, dir[i]) == 1)
		{
			if(cmd[0] == '\0') {
                strcpy(cmd, dir[i]); //if first match, copy it
                strlcat(cmd, " ", NUM_COLS);
            } else {
                /* change string to only be the first common chars of current
                   and prev matches */
				len = strcmp(dir[i], cmd);
				for(j=0; j < NUM_COLS; j++) cmd[j] = '\0'; //clear cmd
				strncpy(cmd, dir[i], len);
			}
		}
        kfree(dir[i]);
	}
	
	//if a completion was found, replace text with it
	if(cmd[0] != '\0')
	{
		//remove the incomplete user input
		i = current_terminal->keyboard_buffer_size - 1;
		while(current_terminal->keyboard_buffer[i] != ' ' && i >= 0)
		{
			current_terminal->keyboard_buffer[i] = '\0';
			i--;	
		}
		
		//append the new complete command
		i++;
		len = strlen(cmd);
		for(j=0; j < len; j++)
		{
			current_terminal->keyboard_buffer[i] = cmd[j];
			i++;
		}	
		current_terminal->keyboard_buffer_size = strlen(current_terminal->keyboard_buffer);
		current_terminal->keyboard_buffer_pos = strlen(current_terminal->keyboard_buffer);
	}

	return;
}

/**
 * Keyboard read function (for read syscall).
 * @param file Unused.
 * @param buf The buffer into which we should copy the data from the keyboard.
 * @param nbytes The number of bytes to copy.
 */
int32_t keyboard_read(file_info_t *file, uint8_t* buf, int32_t nbytes)
{
    int32_t bytes_read = 0;
    // Loop until we've read from the keyboard.
    sti();
    cli();
    while( (current_terminal != current_process->terminal) ||
            (current_terminal->keyboard_read_flag == 0))
    {
        cli();
        schedule();
        sti();
    }
    cli();

    // Basically, strncpy.
    int32_t i = 0;
    while(current_terminal->keyboard_read_buffer[i] != '\0' && i < nbytes) {
        buf[i] = current_terminal->keyboard_read_buffer[i];
        bytes_read++;
        i++;
    }

    while(i < nbytes) {
        buf[i] = '\0';
        i++;
    }

    // Reset the keyboard read flag.
    current_terminal->keyboard_read_flag = 0;

    return bytes_read;
}

/**
 * Keyboard write function (for write syscall).
 * @param file Unused.
 * @param buf The buffer from which we should copy data from and print.
 * @param nbytes The number of bytes to copy.
 */
int32_t keyboard_write(file_info_t *file, const int8_t* buf, int32_t nbytes)
{
    int i;
    uint8_t theChar;
    int32_t bytes_written = 0;
    coord_t *keyboard_start = &current_process->terminal->keyboard_start_coord;

    for(i = 0; i < nbytes; i++)
    {
        theChar = buf[i];
        if(theChar != '\0') {
			// If we're looking at this terminal, print the character.
            if(current_terminal == current_process->terminal)
            {
                scrollback_offset = 0;
                set_scrollback_page(0);
                hide_cursor();
                putc(theChar);
                show_cursor();
            }
			// If we're not, print it to a backing page.
            else
            {
                putc_to_backing(theChar, current_process->terminal);
            }

			// Modify coordinates.
            keyboard_start->x += 1;
            if(keyboard_start->x == NUM_COLS || theChar == '\n')
            {
                keyboard_start->x = 0;
                keyboard_start->y += 1;
                if (keyboard_start->y >= NUM_ROWS) {
                    keyboard_start->y = NUM_ROWS - 1;
                }
            }

            bytes_written++;
        } else {
            break;
        }
    }

	// Update the cursor if the terminal is visible.
    if (current_terminal == current_process->terminal) {
        update_cursor();
    }

    return bytes_written;
}

/**
 * Print the current keyboard buffer to the screen.
 */
void reprint_keyboard_buffer() {
    set_screen_coordinates(current_terminal->keyboard_start_coord.x,
            current_terminal->keyboard_start_coord.y);
    puts_wrap(current_terminal->keyboard_buffer);
}

/**
 * Clears all of the memory for a given terminal's backing page.
 */
void clear_terminal_backing_page(terminal_info_t *terminal)
{
    int32_t blank = (current_attrib << 8) | ' ';
    memset_word(terminal->video_memory, blank, NUM_ROWS*NUM_COLS);
    set_screen_coordinates(0, 0);
}

/**
 * Scroll a backing page up for the given terminal.
 * Changed to video_memory_base.  Is this right?
 */
void scroll_backing(terminal_info_t *terminal)
{
	int x, y;
    int8_t* scrollback_base = terminal->video_memory_base - 2*NUM_COLS*NUM_ROWS*MAX_SCROLLBACK_OFFSET;
    int8_t* current_position;
    // Scroll the scrollback buffer lines.
    hide_cursor();
    for(current_position = scrollback_base; current_position < terminal->video_memory_base + NUM_COLS; current_position++)
    {
        *current_position = *(current_position + NUM_COLS * 2);
    }
    show_cursor();

    // Scroll the "normal lines."
	for(y=0; y < (NUM_ROWS - 1); y++) {
		for(x=0; x < NUM_COLS; x++) {
            terminal->video_memory_base[ (NUM_COLS * y + x) << 1 ] =
                terminal->video_memory_base[ (NUM_COLS * (y+1) + x) << 1 ];
        }
    }
    // Clear the bottom line.
	for(x=0; x < NUM_COLS; x++) {
        terminal->video_memory_base[ (NUM_COLS * (NUM_ROWS - 1) + x) << 1 ] = 0x00;
    }
}

/**
 * Adds a character to the given backing page.
 * @param c The character to write.
 * @param terminal The terminal to use.
 */
    void
putc_to_backing(uint8_t c, terminal_info_t *terminal)
{
    uint8_t *x = &terminal->current_position.x;
    uint8_t *y = &terminal->current_position.y;
	
	// Handle newlines.
    if(c == '\n' || c == '\r') {
        *y += 1;

		// Scroll if necessary.
        if(*y >= NUM_ROWS)
        {
            scroll_backing(terminal);
            *y -= 1;
        }
        *x=0;
    }
    else 
    {
		// Modify the backing page.
        uint32_t base_addr = (uint32_t)terminal->video_memory_base;
        *(uint8_t *)(base_addr + ((NUM_COLS* *y + *x) << 1)) = c;
        *(uint8_t *)(base_addr + ((NUM_COLS* *y + *x) << 1) + 1) =
            current_attrib;
        *x += 1;
    }
    if(*x == NUM_COLS && *y == NUM_ROWS - 1)
    {
		// Scroll if necessary.
        scroll_backing(terminal);
        *x = 0;
    }
}

/**
 * Initialize all terminal structs.
 */
int32_t init_terminals()
{
    terminals = kmalloc((MAX_SCROLLBACK_OFFSET + 1) * NUM_TERMINALS * sizeof(terminal_info_t));
    current_terminal = terminals;
    int i, j;
    for(i = 0; i < NUM_TERMINALS; i++)
    {
        terminals[i].index = i;

        terminals[i].keyboard_read_flag = 0;

        terminals[i].keyboard_start_coord.x = 0;
        terminals[i].keyboard_start_coord.y = 0;

        terminals[i].keyboard_buffer_pos = 0;
        terminals[i].keyboard_buffer_size = 0;

        for(j = 0; j < BUFFER_SIZE; j++)
        {
            terminals[i].keyboard_buffer[j] = 0;
            terminals[i].keyboard_read_buffer[j] = 0;
        }

        terminals[i].current_position.x = 0;
        terminals[i].current_position.y = 0;

        terminals[i].history_size = 0;
        terminals[i].history_curr = 0;

        terminals[i].video_memory = kmalloc(2*NUM_COLS*NUM_ROWS*(MAX_SCROLLBACK_OFFSET + 1)) + 2*NUM_COLS*NUM_ROWS*MAX_SCROLLBACK_OFFSET;
        terminals[i].video_memory_base = terminals[i].video_memory;
        clear_terminal_backing_page(&terminals[i]);
    }
	add_left_click(line_click);
    return 0;
}

/**
 * Keyboard open function stub for syscall handler.
 */
int32_t keyboard_open()
{
    return 0;
}

/**
 * Keyboard close function stub for syscall handler.
 */
int32_t keyboard_close(file_info_t *file)
{
    return 0;
}

/**
 * Switches terminals to the specified terminal.
 *
 * Saves the current process's video memory and restores the new process's video memory
 * to/from backing pages.
 * @param terminal The terminal to switch to.
 */
void switch_terminals(terminal_info_t *terminal)
{
    uint32_t flags;
    LOCK();
    block_interrupts(&flags);
    // hide the cursor to avoid having it saved to the backing page
    hide_cursor();
    if (terminal == NULL) {
        current_terminal = NULL;
    }
    if (current_terminal != NULL) {
        // Save the coordinates of the current terminal so we can return to
        // something useful later.
        current_terminal->current_position = read_screen_coordinates();

        // Remap the current terminal's (process's) video memory.
        map_backing_page(current_terminal);

        // If the current terminal had called vidmap, remap that page to the
        // backing page as well.    
        if(process_in_terminal[current_terminal->index] != NULL &&
                process_in_terminal[current_terminal->index]->vidmap_flag == 1)
        {
            map_4kb_page((uint32_t) current_terminal->video_memory, MB(256),
                    process_in_terminal[current_terminal->index]->pid,
                    KernelPrivilege, 0);
        }

        // restore this terminal's background
        set_segment_inactive(current_terminal->index + 2);
    }
    // Switch to a new terminal.
    current_terminal = terminal;

    // Unmap the new terminal's backing page.
    unmap_backing_page(terminal);

    // If the current terminal had called vidmap, remap that page to the front as well.
    if(process_in_terminal[current_terminal->index] != NULL &&
            process_in_terminal[current_terminal->index]->vidmap_flag == 1)
    {
        map_4kb_page((uint32_t) VIDEO, MB(256),
                process_in_terminal[current_terminal->index]->pid,
                KernelPrivilege, 0);
    }

	// Restore coordinates.
    set_screen_coordinates(current_terminal->current_position.x,
            current_terminal->current_position.y);

    // Show the cursor on the new terminal
    show_cursor();
    set_segment_active(current_terminal->index + 2);

    UNLOCK();
    restore_interrupts(flags);
}

/**
 * "Activates" a new terminal and return a pointer to it.
 * @return A pointer to the new terminal.
 */
terminal_info_t *new_terminal(void)
{
    int i;
    for (i = 0; i < NUM_TERMINALS; i++) {
        if (process_in_terminal[i] == NULL || process_in_terminal[i]->pid == 0) {
            return &terminals[i];
        }
    }
    return NULL;
}

/**
 * Copies all of the video memory from the current terminal into a backing page. 
 * @param terminal The relevant terminal.
 */
void map_backing_page(terminal_info_t *terminal)
{
    // Copy all the data into the backing page.
    memcpy(terminal->video_memory, (int8_t*)VIDEO, NUM_COLS * NUM_ROWS * 2);
}

/**
 * Copies all of the video memory from the current terminal from a backing page. 
 * @param terminal The relevant terminal.
 */
void unmap_backing_page(terminal_info_t *terminal)
{
    // Copy the contents of the backing page into video memory.
    memcpy((int8_t*)VIDEO, terminal->video_memory, NUM_COLS * NUM_ROWS * 2);
}

void map_base_page(terminal_info_t *terminal)
{
    // Copy all the data into the backing page.
    memcpy(terminal->video_memory_base, (int8_t*)VIDEO, NUM_COLS * NUM_ROWS * 2);
}

void unmap_base_page(terminal_info_t *terminal)
{
    // Copy the contents of the backing page into video memory.
    memcpy((int8_t*)VIDEO, terminal->video_memory_base, NUM_COLS * NUM_ROWS * 2);
}

/**
 * Changes video memory to be (offset) scrollback pages from the given one.
 */
void adjust_scrollback_page(int32_t offset)
{
    hide_cursor();
    if(scrollback_offset == 0)
    {
        map_backing_page(current_terminal);
    }
    if((offset + scrollback_offset) < 0)
    {
        scrollback_offset = 0;
    }
    else if((offset + scrollback_offset) > MAX_SCROLLBACK_OFFSET)
    {
        scrollback_offset = MAX_SCROLLBACK_OFFSET;
    }
    else
    {
        scrollback_offset += offset;
    }
    // Map the current backing page, just to be safe.
    load_scrollback_page(scrollback_offset);
    show_cursor();
}

/**
 * Sets the scrollback page for the current terminal to the given value.
 */
void set_scrollback_page(int32_t value)
{
    hide_cursor();
    if(scrollback_offset == 0)
    {
        map_backing_page(current_terminal);
    }
    if(value < 0)
    {
        scrollback_offset = 0;
    }
    else if(value > MAX_SCROLLBACK_OFFSET)
    {
        scrollback_offset = MAX_SCROLLBACK_OFFSET;
    }
    else
    {
        scrollback_offset = value;
    }
    load_scrollback_page(scrollback_offset);
    show_cursor();
}

/**
 * Loads the scrollback page for a given process.
 */
void load_scrollback_page(int32_t offset)
{
    current_terminal->video_memory = current_terminal->video_memory_base - 2*NUM_COLS*NUM_ROWS*offset;
    unmap_backing_page(current_terminal);
}

/**
 * Allows a mouse click on the current line to change cursor position. 
 * @param x,y coordinates of mouse click
 */
void line_click(int x, int y)
{
	cli();
	coord_t start; //marks where start of current buffer is on screen
	start.x = current_terminal->keyboard_start_coord.x;
	start.y = current_terminal->keyboard_start_coord.y;
		
	uint32_t buffer_height = current_terminal->keyboard_buffer_size / (NUM_COLS - start.x);
	uint32_t buffer_rem = current_terminal->keyboard_buffer_size % (NUM_COLS - start.x);
	int y_off = y - (int)start.y;
	
	if(y_off < 0 || y_off > buffer_height) return;  //if line clicked is not one being worked on
	if(y_off == 0 && x < start.x) return;	//if the prompt is clicked
	
	if(y_off == 0)
	{
		if(buffer_height == 0 && x > (start.x + buffer_rem)) x = start.x + buffer_rem; //click beyond buffer on first line and buffer is less than a line long
	}	
	else if(y_off == buffer_height && x > buffer_rem) x = buffer_rem;  //click beyond buffer on last line of buffer that's more than one line long
	
	//clear cursor image
	clear_char_attrib(start.x + current_terminal->keyboard_buffer_pos, start.y);
	
	//change position in buffer
	current_terminal->keyboard_buffer_pos = y_off * (NUM_COLS) + x - start.x;

	//move the cursor.
    set_screen_coordinates(x,y);
	sti();
}

