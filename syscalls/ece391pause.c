#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"
#include "../student-distrib/soundctrl.h"

#define SBUFSIZE 33

int main ()
{
    ece391_soundctrl(CTRL_PAUSE, 0);

    return 0;
}
