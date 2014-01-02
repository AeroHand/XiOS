#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"
#include "../student-distrib/soundctrl.h"

#define BUFSIZE 33

int main ()
{
    uint8_t buf[BUFSIZE];

    if (0 != ece391_getargs (buf, BUFSIZE)) {
	    ece391_fdputs (1, (uint8_t*)"could not read filename\n");
	    return 3;
    }

    if (ece391_soundctrl(CTRL_PLAY_FILE, (int8_t*) buf) < 0) {
	    ece391_fdputs(1, (uint8_t*)"could not play; is something else playing or did you pass a non-existant file?\n");
    }

    return 0;
}
