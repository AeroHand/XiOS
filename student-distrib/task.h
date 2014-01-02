// vim: ts=4:et:sw=4
#ifndef _TASK_H
#define _TASK_H

#include "lib.h"
#include "fs.h"
#include "x86_desc.h"

/* Include one process for the kernel */
#define MAX_PROCESSES 100
#define MAX_FILES 8

struct process;
struct task;
struct terminal_info;

typedef struct process {
    int32_t pid;
    // virtual memory location of user stack
    void *user_stack;
    // kernel stack for this process to switch back to in privilege switch
    void *kernel_stack;
    // physical address of memory image
    void *page_start;

    file_info_t open_files[MAX_FILES];

    int8_t program[32+1];
    uint8_t args[100];
    int32_t ret_val;
    void* ret_addr;

    registers_t registers;

    // starts at 1 for the shell, incremented on each successive execute call
    int32_t level;
    struct process *parent;
    struct task *task;

    // terminal that is mapped to this process.
    struct terminal_info *terminal;
    // check to see if this process has called vidmap (used for switches)
    int32_t vidmap_flag;
} process_t;

extern process_t *current_process;

typedef enum task_status {
    TaskActive,
    TaskIdle,
    TaskSleeping,
} task_status_t;

typedef struct task {
    process_t *process;
    task_status_t status;
    struct task *next;
    struct task *prev;
} task_t;

typedef struct task_queue {
    task_t *head;
    task_t *tail;
    uint32_t num_tasks;
} task_queue_t;

extern process_t* kernel_proc;

void* load_program(int8_t *program, uint8_t *addr);
void* setup_process(int8_t *command);
process_t * kernel_spawn(int8_t *command);
void set_current_process(process_t* process);
void init_processes(void);
process_t* new_process(void);
void close_process(process_t *process);

extern task_queue_t runqueue;
extern process_t *current_process;

void init_taskqueue(task_queue_t *queue);
task_t* add_process(process_t *process, task_queue_t *queue);
void idle_task(task_t *task);
void activate_task(task_t *task);
task_t* next_task(task_queue_t *queue);
task_t* remove_task(task_t *task, task_queue_t* queue);
task_t *pop_head_task(task_queue_t* queue);
task_t *pop_tail_task(task_queue_t* queue);
void push_head_task(task_t* task, task_queue_t* queue);
void push_tail_task(task_t* task, task_queue_t* queue);
void free_task(task_t *task);
void task_switch(task_t* first, task_t* second);
void schedule();
void set_status_bar();

// This should be called BEFORE leave!
#define modify_return_addr(addr)        \
{                                 \
    registers_t old_regs;           \
    save_regs(old_regs);            \
    asm("movl %%eax, 4(%%ebp);"     \
            : /* no outputs */      \
            : "a"(addr)             \
            :"memory");             \
    restore_regs(old_regs);         \
}

// This should be called BEFORE leave!
#define modify_cs(addr)                 \
{                                 \
    registers_t old_regs;           \
    save_regs(old_regs);            \
    asm("movl %%eax, 8(%%ebp);"     \
            : /* no outputs */      \
            : "a"(addr)             \
            :"memory");             \
    restore_regs(old_regs);         \
}

// This should be called BEFORE leave!
#define modify_eflags(addr)             \
{                                 \
    registers_t old_regs;           \
    save_regs(old_regs);            \
    asm("movl %%eax, 12(%%ebp);"    \
            : /* no outputs */      \
            : "a"(addr)             \
            :"memory");             \
    restore_regs(old_regs);         \
}

// This should be called BEFORE leave!
#define modify_esp(addr)                \
{                                 \
    registers_t old_regs;           \
    save_regs(old_regs);            \
    asm("movl %%eax, 16(%%ebp);"    \
            : /* no outputs */      \
            : "a"(addr)             \
            :"memory");             \
    restore_regs(old_regs);         \
}

// This should be called BEFORE leave!
#define modify_ss(addr)                 \
{                                 \
    registers_t old_regs;           \
    save_regs(old_regs);            \
    asm("movl %%eax, 20(%%ebp);"    \
            : /* no outputs */      \
            : "a"(addr)             \
            :"memory");             \
    restore_regs(old_regs);         \
}

#define expand_iret_frame()             \
{                                 \
    registers_t old_regs;           \
    save_regs(old_regs);            \
    asm("movl %%esp, %%ebx;         \
            movl %%ebp, %%ecx;      \
            addl %%ecx, $16;        \
            movl (%%ebx), %%eax;    \
            movl %%eax, -8(%%ebx);  \
            addl %%ebx, $4;         \
            cmpl %%esp, %%ecx;      \
            jb	$-4;"               \
            : /* no outputs */      \
            : /* no inputs */       \
            :"memory");             \
    restore_regs(old_regs);         \
}

#define PUSH_USER() {                        \
    asm ("xorl %%eax, %%eax;                    \
            movw $0x2B, %%ax;                   \
            pushl %%eax;                        \
            pushl %0;                           \
            pushfl;                             \
            popl %%eax;                         \
            orl $0x200, %%eax;                  \
            pushl %%eax;                        \
            movw $0x23, %%ax;                   \
            pushl %%eax;"                       \
            : /* no outputs */                  \
            : "r"(current_process->user_stack)  \
            : "eax"                             \
        );                                      \
}

#define PUSH_KERNEL() {                             \
    asm ("                                          \
            pushfl;                                 \
            movw $0x10, %%ax;                       \
            pushl %%eax;"                           \
            : /* no outputs */                      \
            : "r"(current_process->registers.esp)   \
            : "eax"                                 \
        );                                          \
}

#define PUSH_RETURN_ADDRESS(addr) {      \
    asm ("pushl %0;"                        \
            : /* no outputs */              \
            : "r" (addr)                    \
        );                                  \
}


#endif
