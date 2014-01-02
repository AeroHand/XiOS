//vim:ts=4:sw=4:et

#include "shutdown.h"
#include "lib.h"
#include "syscall.h"

/**
 * shutdown the system
 *
 * waits for user to hit enter first
 * only works for qemu 
 *
 * @param message a message to print before waiting for enter
 */
void shutdown(int8_t* message)
{
    uint8_t bucket[10];
    uint32_t length = strlen(message);
		syscall_write(STDOUT_FD, (uint8_t*) message, length);
		syscall_read(STDIN_FD, bucket, 10);
    outw(0x2000, 0xb004);
}
