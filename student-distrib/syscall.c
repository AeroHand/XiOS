// vim: tw=80:ts=4:sw=4:et
#include "lib.h"
#include "syscall.h"
#include "fs.h"
#include "keyboard.h"
#include "task.h"
#include "paging.h"
#include "rtc.h"
#include "x86_desc.h"
#include "shutdown.h"
#include "sb16.h"
#include "soundctrl.h"

static file_ops_t terminal_funcs = {.read_func = keyboard_read,
    .write_func = keyboard_write,
    .open_func = keyboard_open,
    .close_func = keyboard_close,
};

static file_ops_t fs_funcs = {.read_func = file_read,
    .write_func = fs_write,
    .open_func = fs_open,
    .close_func = fs_close,
};

static file_ops_t dir_funcs = {.read_func = directory_read,
    .write_func = fs_write,
    .open_func = fs_open,
    .close_func = fs_close,
};

static file_ops_t rtc_funcs = {.read_func = rtc_read,
    .write_func = rtc_write,
    .open_func = rtc_open,
    .close_func = rtc_close,
};

int32_t find_new_fd();

/**
 * handle the syscall interrupt
 *
 * dispatches to the appropriate function to handle the system call
 */
void syscall_handler() {
    // save a bunch of state
    registers_t regs;
    save_regs(regs);

    // get arguments from %ebx, %ecx, %edx
    uint32_t num;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
    // the register allocations specified for the inputs ask GCC to store the
    // registers into their coprint rrect local variables, so no body is needed
    // (= specifies write-only)
    asm(""
        : "=a"(num), "=b"(arg1), "=c"(arg2), "=d"(arg3)
        : /* no inputs */);
    // call the handler of the appropriate system call

    // load USER_CS
    asm volatile ("           \
        movl %0, %%edx      \n\
        movw %%dx, %%ds     \n\
        movw %%dx, %%es"
        :
        :"r"(KERNEL_DS)
        :"%edx" );

    uint32_t ret;
    switch(num) {
        // this sycall never returns
        case SYSCALL_HALT:
            syscall_halt( (uint8_t) (arg1 & 0xFF) );
        case SYSCALL_EXECUTE:
            ret = syscall_execute((uint8_t*) arg1);
            break;
        case SYSCALL_OPEN:
            ret = syscall_open((uint8_t*)arg1);
            break;
        case SYSCALL_READ:
            ret = syscall_read(arg1, (uint8_t*)arg2, arg3);
            break;
        case SYSCALL_WRITE:
            ret = syscall_write(arg1, (uint8_t*)arg2, arg3);
            break;
        case SYSCALL_CLOSE:
            ret = syscall_close((int32_t) arg1);
            break;
        case SYSCALL_VIDMAP:
            ret = syscall_vidmap((uint8_t**) arg1);
            break;
        case SYSCALL_GETARGS:
            ret = syscall_getargs((uint8_t *) arg1, (int32_t) arg2);
            break;
        case SYSCALL_SET_HANDLER:
            ret = syscall_set_handler(arg1, (void *) arg2);
            break;
        case SYSCALL_SIGRETURN:
            ret = syscall_sigreturn();
            break;
        case SYSCALL_SHUTDOWN:
            ret = syscall_shutdown();
            break;
        case SYSCALL_SOUNDCTRL:
            ret = syscall_soundctrl(arg1, (int8_t*)arg2);
            break;
        default:
            ret = -1;
    }

    // restore a bunch of state
    restore_regs(regs);

    // set %eax to the return value of the kernel handler (to send to user)
    asm volatile ("" : : "a"(ret));
    asm volatile ("           \
        leave               \n\
        iret");
}

int32_t generate_syscall(int32_t num, int32_t arg1, int32_t arg2, int32_t arg3) {
    asm(""
        : /* no inputs */
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3) );
    asm volatile ("     \
        int $0x80"
        :
        :
        : "cc", "memory" );
    int32_t ret;
    asm (""
        :"=a"(ret) );
    return ret;
}

/**
 * open system call
 *
 * creates a file struct for the current process and handles opening the
 * requested filename (which may be a device)
 *
 * @return a file descriptor integer for use in future system calls
 */
int32_t syscall_open(uint8_t* filename) {
    int32_t fd;
    if (strncmp((int8_t*)filename, "/dev/stdin", 100) == 0) {
        file_info_t stdin_info;
        stdin_info.file_ops = &terminal_funcs;
        stdin_info.inode_ptr = NULL;
        stdin_info.pos = 0;
        stdin_info.can_read = 1;
        stdin_info.can_write = 0;
        stdin_info.in_use = 1;
        stdin_info.type = FileTerminal;
        current_process->open_files[STDIN_FD] = stdin_info;
        fd = STDIN_FD;
    } else if (strncmp((int8_t*)filename, "/dev/stdout", 100) == 0) {
        file_info_t stdout_info = {
            .file_ops = &terminal_funcs,
            .inode_ptr = NULL,
            .pos = 0,
        };
        stdout_info.can_read = 0;
        stdout_info.can_write = 1;
        stdout_info.type = FileTerminal;
        stdout_info.in_use = 1;
        current_process->open_files[STDOUT_FD] = stdout_info;
        fd = STDOUT_FD;
    } else if (strncmp((int8_t*)filename, "/dev/rtc", 100) == 0)  {
        file_info_t rtc_info = {
            .file_ops = &rtc_funcs,
            .inode_ptr = NULL,
            .pos = 0,
        };
        rtc_info.can_read = 1;
        rtc_info.can_write = 1;
        rtc_info.type = FileRTC;
        rtc_info.in_use = 1;
        int32_t file_num = find_new_fd();
        if (file_num < 0) {
            return -1;
        } else {
            current_process->open_files[file_num] = rtc_info;
            fd = file_num;
        }
    } else {
        dentry_t dentry;
        if (read_dentry_by_name(filename, &dentry)) {
            return -1;
        }
        file_info_t fs_info;
        fs_info.inode_ptr = get_inode_ptr(dentry.inode);
        fs_info.pos = 0;
        if (dentry.type == DENTRY_DIRECTORY) {
            fs_info.file_ops = &dir_funcs;
            fs_info.can_read = 1;
            fs_info.can_write = 0;
            fs_info.type = FileRegular;
        } else if (dentry.type == DENTRY_FILE) {
            fs_info.file_ops = &fs_funcs;
            fs_info.can_read = 1;
            fs_info.can_write = 0;
            fs_info.type = FileRegular;
        } else if (dentry.type == DENTRY_RTC) {
            fs_info.file_ops = &rtc_funcs;
            fs_info.can_read = 1;
            fs_info.can_write = 1;
            fs_info.type = FileRTC;
        }
        fs_info.in_use = 1;
        int32_t file_num = find_new_fd();
        if (file_num < 0) {
            return -1;
        } else {
            current_process->open_files[file_num] = fs_info;
            fd = file_num;
        }
    }

    current_process->open_files[fd].file_ops->open_func();
    return fd;
}

/**
 * execute system call
 *
 * idles the current process and begins executing a new command on top of it
 * also handles returning to the process when the command has finished
 *
 * @param command a command line to execute, including arguments
 * @return the return value of the process executed, which comes from a halt()
 * system call
 */
int32_t syscall_execute(const uint8_t *command) {
    void* start_address;
    save_regs(current_process->registers);
    start_address = setup_process((int8_t*)command);
    if(start_address == NULL) {
        return -1;
    }
    idle_task(current_process->parent->task);
    set_status_bar();
    asm volatile ("                           \
        leal syscall_restore_regs, %%eax    \n\
        movl %%eax, %0"
        :"=m"(current_process->parent->ret_addr)
        :
        :"eax" );
    PUSH_USER();
    PUSH_RETURN_ADDRESS(start_address);
    asm volatile ("iret");
    asm volatile ("syscall_restore_regs:");
    restore_regs(current_process->registers);
    activate_task(current_process->task);
    set_status_bar();
    return current_process->ret_val;
}

/**
 * read system call
 *
 * read from a file descriptor into a provided buffer
 *
 * @param fd a file descriptor previously retrieved with open() specific to the
 * current process
 * @param buf a pointer to a buffer
 * @nbytes the length of buf (no more than nbytes will be written), or simply
 * the number of bytes requested
 * @return number of bytes read on success -1 on failure
 */
int32_t syscall_read(int32_t fd, const uint8_t* buf, int32_t nbytes) {
    if (valid_fd(fd) && current_process->open_files[fd].can_read) {
        file_info_t* f = &(current_process->open_files[fd]);
        int32_t bytes_read = f->file_ops->read_func(f, (uint8_t*)buf, nbytes);
        return bytes_read;
    }
    return -1;
}

/**
 * write system call
 *
 * writes data to a file specified by file descriptor
 *
 * @param fd a file descriptor previously retrieved with open() specific to the
 * current process
 * @param buf a pointer to the buffer holding the data
 * @param nbytes the number of bytes to try to write (though fewer will be
 * written if the file has no more available)
 * @return 0 on success, -1 on failure
 */
int32_t syscall_write(int32_t fd, const uint8_t* buf, int32_t nbytes) {
    if (valid_fd(fd) && current_process->open_files[fd].can_write) {
        file_info_t* f = &(current_process->open_files[fd]);
        f->file_ops->write_func(f, (int8_t*)buf, nbytes);
        return 0;
    }
    return -1;
}

/**
 * halt system call
 *
 * stop the current process and return to its parent
 *
 * this function does not return to the current process
 *
 * @param status the value to return to the parent process's execute() system
 * call
 */
void syscall_halt(uint8_t status) {
    process_t *old_process = current_process;
    assert(old_process->parent != NULL);
    void* ret_addr = old_process->parent->ret_addr;
    set_current_process(current_process->parent);
    if (current_process->pid == 0) {
        process_in_terminal[old_process->terminal->index] = current_process;
        if (old_process->terminal == current_terminal) {
            clear();
        }
    }
    close_process(old_process);
    set_status_bar();
    current_process->ret_val = status;
    asm ("      \
        jmp *%0"
        : /* no outputs */
        : "r" (ret_addr) );
}

/**
 * close system call
 *
 * closes a file, making the fd invalid and available for future use, and
 * allowing drivers to clean up
 *
 * refuses to close STDIN and STDOUT
 *
 * @param fd the file descriptor
 * @return 0 on success, -1 on failure
 */
int32_t syscall_close(int32_t fd) {
    if (valid_fd(fd) && fd != STDIN_FD && fd != STDOUT_FD) {
        file_info_t *f = &current_process->open_files[fd];
        f->file_ops->close_func(f);
        f->in_use = 0;
        return 0;
    }
    return -1;
}

/**
 * getargs system call
 *
 * gather the arguments the current process was called with
 *
 * @param buf the buffer to store the arguments in
 * @param nbytes the maximum size of the buffer
 *
 * @return -1 if there are no arguments to return, 0 otherwise
 */
int32_t syscall_getargs(uint8_t *buf, uint32_t nbytes) {
    if (strlen((int8_t*)current_process->args) == 0) {
        return -1;
    }
    strncpy((int8_t*)buf, (int8_t*)current_process->args, nbytes);
    return 0;
}

int32_t syscall_set_handler(int32_t signum, void* handler_address)
{
    return -1;
}

int32_t syscall_sigreturn(void)
{
    return -1;
}

/**
 * vidmap system call
 *
 * maps video memory into the current process's virtual memory space, reporting
 * where it was placed
 *
 * @param screen_start a pointer to where the start of video memory should be
 * stored
 * @return 0 on success, -1 if the given pointer is invalid
 */
int32_t syscall_vidmap (uint8_t** screen_start)
{
    // Check for out of range memory.
    if((uint32_t)screen_start < MB(128) || (uint32_t)screen_start >= MB(132) - 4 )
    {
        return -1;
    }
    map_4kb_page(0xB8000, MB(256), current_process->pid, UserPrivilege, 1);

    *screen_start = (uint8_t *) MB(256);

    current_process->vidmap_flag = 1;

    return 0;
}

int32_t syscall_shutdown(void) {
    sb16_stop_playback();
    clear();
    printSB();
    play_wav("shutdown_sb.wav");
    shutdown("Press enter to shutdown...\n");
    return -1;
}

int32_t syscall_soundctrl(int32_t function, int8_t *filename) {
    switch (function) {
        case CTRL_PLAY_FILE:
            return play_wav(filename);
            break;
        case CTRL_PAUSE:
            sb16_pause_playback();
            break;
        case CTRL_RESUME:
            sb16_resume_playback();
            break;
        case CTRL_STOP:
            sb16_stop_playback();
            break;
        default:
            return -1;
    }
    return 0;
}

/**
 * check the a file descriptor is valid in the context of the current process
 *
 * confirms it is within bounds and in use
 */
int8_t valid_fd(int32_t fd) {
    if (0 <= fd && fd < 8 && current_process->open_files[fd].in_use)
        return 1;
    else
        return 0;
}

/**
 * searches the list of file descriptors for the current process to find the
 * next one available
 *
 * does not mark it used
 *
 * @return the file descriptor if one was found, -1 if all are in use
 */
int32_t find_new_fd() {
    int32_t fd;
    for (fd = 0; fd < 8; fd++) {
        if (current_process->open_files[fd].in_use == 0) {
            return fd;
        }
    }
    return -1; 
}
