//vim:ts=4:sw=4:et
#ifndef __SYSCALL_H
#define __SYSCALL_H

#define SYSCALL_HALT 1
#define SYSCALL_EXECUTE 2
#define SYSCALL_READ 3
#define SYSCALL_WRITE 4
#define SYSCALL_OPEN 5
#define SYSCALL_CLOSE 6
#define SYSCALL_GETARGS 7
#define SYSCALL_VIDMAP 8
#define SYSCALL_SET_HANDLER 9
#define SYSCALL_SIGRETURN 10
#define SYSCALL_SHUTDOWN 11
#define SYSCALL_SOUNDCTRL 12

#define STDIN_FD 0
#define STDOUT_FD 1
void syscall_handler(void);
int32_t generate_syscall(int32_t num, int32_t arg1, int32_t arg2, int32_t arg3);
int32_t syscall_execute(const uint8_t* command);
int32_t syscall_read(int32_t fd, const uint8_t* buf, int32_t nbytes);
int32_t syscall_write(int32_t fd, const uint8_t* buf, int32_t nbytes);
void syscall_halt(uint8_t status);
int32_t syscall_open(uint8_t* filename);
int32_t syscall_close(int32_t fd);
int32_t syscall_vidmap (uint8_t** screen_start);
int32_t syscall_getargs(uint8_t *buf, uint32_t nbytes);
int32_t syscall_set_handler(int32_t signum, void* handler_address);
int32_t syscall_sigreturn(void);
int32_t syscall_shutdown(void);
int32_t syscall_soundctrl(int32_t function, int8_t *filename);
int8_t valid_fd(int32_t fd);


#endif
