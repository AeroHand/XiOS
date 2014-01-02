// vim: tw=80:ts=4:sw=4:et
#include "lib.h"
#include "task.h"
#include "syscall.h"
#include "paging.h"
#include "mem.h"
#include "keyboard.h"
#include "status.h"

#define FILE_HEADER_SIZE 40

process_t* calc_pcb_address(int32_t pid);
uint8_t* calc_kstack_address(int32_t pid);
uint8_t* calc_ustack_address(int32_t pid);
uint8_t* calc_program_start(int32_t pid);

task_queue_t runqueue;
process_t *kernel_proc;

extern process_t* process_in_terminal[NUM_TERMINALS];

process_t* current_process;

/**
 * Loads a program from disk into a specified address of memory
 *
 *     Checks that the file has:
 *     - a valid file type (DENTRY_FILE)
 *     - enough room for a 40B "header"
 *     - the magic first 4 bytes for elf
 *     - Validate that the whole file was read
 *
 * @param program name of a file to load
 * @param addr location in physical memory to load the file to
 * @return the starting virtual address of the executable on seccess, NULL on
 * failure
 */
void* load_program(int8_t *program, uint8_t *addr) {
    dentry_t dentry;
    inode_t *inode_ptr;
    uint32_t file_length;
    void* start_address;

    if (read_dentry_by_name((uint8_t*)program, &dentry) == -1) {
        return NULL;
    }
    //check that this is a regular file, it should be
    if(dentry.type != DENTRY_FILE) {
        return NULL;
    }
    inode_ptr = get_inode_ptr(dentry.inode);
    file_length = inode_ptr->length;

    //file should have 40B "header" (FILE_HEADER_SIZE)
    if(read_data(inode_ptr, 0, addr, FILE_HEADER_SIZE) < FILE_HEADER_SIZE) {
        return NULL;
    }
    //check for magic number
    if(*((uint32_t*)addr) != 0x464c457f) {
        return NULL;
    }
    //find starting address for executable, located at bytes 24-27
    start_address = *((void**)(addr + 24));
    //read the rest of the file, ensure that all is read
    if(read_data(inode_ptr, FILE_HEADER_SIZE,
                addr + FILE_HEADER_SIZE, file_length - FILE_HEADER_SIZE)
            < file_length - FILE_HEADER_SIZE) {
        return NULL;
    }

    return start_address;
}

/**
 * initialize the process subsystem
 *
 * initializes the runqueue with the current single process, the kernel
 */
void init_processes(void) {
    // set up runqueue first
    init_taskqueue(&runqueue);
    // set up kernel PCB
    kernel_proc = calc_pcb_address(0);
    int i;

    kernel_proc->pid = 0;
    kernel_proc->user_stack = NULL;
    kernel_proc->kernel_stack = calc_kstack_address(0);
    kernel_proc->page_start = NULL;
    for(i = 0; i < MAX_FILES; i++) {
        kernel_proc->open_files[i].in_use = 0;
    }
    kernel_proc->ret_val = 0;
    kernel_proc->level = 0;
    kernel_proc->parent = NULL;
    kernel_proc->terminal = NULL;
    kernel_proc->vidmap_flag = 0;
    add_process(kernel_proc, &runqueue);
    set_current_process(kernel_proc);

    syscall_open((uint8_t*)"/dev/stdin");
    syscall_open((uint8_t*)"/dev/stdout");
}

/**
 * Setup a process
 *
 * "allocates" a process by pointing to space for a PCB in memory
 * parses a program and arguments from a command
 * loads the program into memory from disk
 * adds the process to the runqueue
 * sets the process to be the current one
 *
 * @param command a command line to execute (ie, program + args)
 * @return start address of the program
 */
void* setup_process(int8_t *command) {
    process_t* process;
    void* start_address; //start address for program

    int i;

    process = new_process();
    if(process == NULL) {
        return NULL;
    }
    for (i = 0; i < 32; i++) {
        if (command[i] == ' ' || command[i] == '\0') {
            break;
        }
        process->program[i] = command[i];
    }
    process->program[i] = '\0';

    // Skip over the space if we're copying to args.
    while(command[i] == ' ') {
        i++;
    }
    int argi = 0;
    while(command[i] != '\0')
    {
        process->args[argi] = command[i];
        i++;
        argi++;
    }
    process->args[argi] = '\0';

    load_pages(process->pid);
    start_address = load_program(process->program, process->page_start);
    load_pages(current_process->pid);
    if(start_address == NULL)
    {
        return NULL;
    }
    add_process(process, &runqueue);
    set_current_process(process);
    syscall_open((uint8_t*)"/dev/stdin");
    syscall_open((uint8_t*)"/dev/stdout");

    return start_address;
}

/**
 * Find a new pid and initialize that process
 *
 * @return pointer to the process_t (PCB) found
 */
process_t* new_process(void) {
    int pid = 0;
    int i;
    task_t *task;
    for(task = runqueue.head; task != NULL; task = task->next) {
        if (task->process->pid > pid) {
            pid = task->process->pid;
        }
    }
    pid++;
    if(pid >= MAX_PROCESSES)
    {
        return NULL;
    }
    process_t* process = calc_pcb_address(pid);
    process->pid = pid;
    process->user_stack = calc_ustack_address(pid);
    process->kernel_stack = calc_kstack_address(pid);
    process->page_start = calc_program_start(pid);
    for(i = 0; i < MAX_FILES; i++) {
        process->open_files[i].in_use = 0;
    }
    process->level = current_process->level + 1;
    process->parent = current_process;

    // If process's parent is the shell, add a new terminal.
    if(process->parent->terminal == NULL) {
        process->terminal = new_terminal();
        if (process->terminal == NULL) {
            return NULL;
        }
        switch_terminals(process->terminal);
    } else {
        // Else, inherit.
        process->terminal = process->parent->terminal;
    }

    // Initialize to 0 (we have never called vidmap with this process... yet).
    process->vidmap_flag = 0;

    return process;
}

process_t *kernel_spawn(int8_t *command) {
    process_t* old_process = current_process;
    current_process = kernel_proc;
    void* start_addr = setup_process("shell");
    // couldn't start the shell, just give up
    if (start_addr == NULL) {
        return NULL;
    }
    process_t *new_process = current_process;
    current_process->ret_addr = start_addr;
    save_regs(current_process->registers);
    current_process = old_process;
    return new_process;
}

/**
 * Change the current process
 *
 * also loads the pages and TSS esp0 with appropriate values
 */
void set_current_process(process_t* process) {
    current_process = process;
    load_pages(current_process->pid);
    tss.esp0 = (uint32_t) current_process->kernel_stack;
}

/**
 * close a process, removing it from its runqueue
 */
void close_process(process_t *process) {
    free_task(remove_task(process->task, &runqueue));
}

process_t* calc_pcb_address(int32_t pid) {
    return (process_t*)(MB(8) - 0x2000 * (pid + 1));
}

uint8_t* calc_kstack_address(int32_t pid) {
    return (void*)(MB(8) - 0x2000 * pid);
}

uint8_t* calc_ustack_address(int32_t pid) {
    return (void*)(MB(128) + MB(4));
}

/* finds physical address for the start of the program code */
uint8_t* calc_program_start(int32_t pid) {
    return (void*)(0x848000 + MB(4) * (pid - 1));
}

void init_taskqueue(task_queue_t *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->num_tasks = 0;
}

/* scheduling tasks */
task_t* add_process(process_t* process, task_queue_t *queue) {
    task_t *task = kmalloc(sizeof(task_t));
    task->process = process;
    process->task = task;
    activate_task(task);

    push_head_task(task, queue);

    return task;
}

task_t* remove_task(task_t *task, task_queue_t* queue) {
    // lock the queue
    //cli();
    // handle this case specially since it means there are a lot of unexpected
    // NULL pointers
    if (queue->num_tasks <= 1) {
        if (task == queue->head) {
            queue->head = NULL;
            queue->tail = NULL;
        } else {
            // release the queue
            //sti();
            return NULL;
        }
    } else {
        if (task->prev != NULL) {
            task->prev->next = task->next;
        }
        if (task == queue->head) {
            queue->head = queue->head->next;
            queue->head->prev = NULL;
        }
        if (task->next != NULL) {
            task->next->prev = task->prev;
        }
        if (task == queue->tail) {
            queue->tail = queue->tail->prev;
            queue->tail->next = NULL;
        }
    }
    task->next = NULL;
    task->prev = NULL;
    queue->num_tasks--;
    // release the queue
    //sti();
    return task;
}

task_t *pop_head_task(task_queue_t* queue) {
    return remove_task(queue->head, queue);
}
task_t *pop_tail_task(task_queue_t* queue) {
    return remove_task(queue->tail, queue);
}
void push_head_task(task_t* task, task_queue_t* queue) {
    // lock the queue
    //cli();
    task->prev = NULL;
    if (queue->head == NULL) {
        // this is the first task in queue
        queue->tail = task;
    } else {
        queue->head->prev = task;
    }
    task->next = queue->head;
    queue->head = task;
    queue->num_tasks++;
    // release the queue
    //sti();
    return;
}
void push_tail_task(task_t* task, task_queue_t* queue) {
    // lock the queue
    //cli();
    task->next = NULL;
    if (queue->tail == NULL) {
        // this is the first task in queue
        queue->head = task;
    } else {
        queue->tail->next = task;
    }
    task->prev = queue->tail;
    queue->tail = task;
    queue->num_tasks++;
    // release the queue
    //sti();
    return;
}

/**
 * free a task struct, reclaiming dynamic memory
 *
 * assumes the task has already been popped from the queue (eg,
 * free_task(remove_task(task, &queue)))
 *
 */
void free_task(task_t *task) {
    kfree(task);
}

/**
 * Set a task to be idle (will not be scheduled)
 */
void idle_task(task_t *task) {
    task->status = TaskIdle;
}

/**
 * Make a task active
 *
 * also marks the task's process as its terminal's process
 */
void activate_task(task_t *task) {
    task->status = TaskActive;

    if(task->process != NULL)
    {
        if(task->process->terminal != NULL)
        {
            process_in_terminal[task->process->terminal->index] = task->process;
        }
    }
}

/**
 * get the next task from a queue by popping from the head
 *
 * rotates the queue; the task has been retrieved so is now put at the end of
 * the queue
 */
task_t *next_task(task_queue_t *queue) {
    if (queue == NULL || queue->head == NULL) {
        return NULL;
    }
    task_t *top = pop_head_task(queue);
    push_tail_task(top, queue);
    return top;
}

/**
 * makes the switch from running one task to another
 *
 * @param from_task the task we expect to restore afterwards
 */
void task_switch(task_t* from_task, task_t* to_task) {
    if (from_task == NULL || to_task == NULL) {
        return;
    }
    save_regs(from_task->process->registers);
    void* ret_addr = to_task->process->ret_addr;
    asm (" \
            leal task_switch_back, %%eax; \n\
            movl %%eax, %0; \n \
            "
            : "=m"(from_task->process->ret_addr)
            :
            : "eax");
    set_current_process(to_task->process);
    // We are only going to user code if the task we are switching to has never
    // been scheduled and is just starting
    if(ret_addr == from_task->process->ret_addr) {
        PUSH_KERNEL();
    } else {
        PUSH_USER();
    }
    PUSH_RETURN_ADDRESS(ret_addr);
    asm volatile ("iret");
    asm("task_switch_back:");
    // We only get back here via another task_switch, at which point
    // current_process is set correctly. This is done in this manner to avoid a
    // Catch-22 where we need the stack in order to correctly restore the stack.
    restore_regs(current_process->registers);
}

/**
 * goes through the runqueue and picks a task to switch to
 */
void schedule() {
    task_t *from_task = current_process->task;
    task_t *to_task = NULL;

    // Limit the number of tasks so we can stop if all the runqueue tasks are
    // idle
    uint32_t tasks_remaining = runqueue.num_tasks;
    do {
        to_task = next_task(&runqueue);
        tasks_remaining--;
    } while (to_task != NULL && to_task->status != TaskActive && tasks_remaining >= 0);
    if (to_task != NULL && from_task != to_task) {
        task_switch(from_task, to_task);
    }
}

void set_status_bar() {
    int i;
    for (i = 0; i < NUM_TERMINALS; i++) {
        set_segment_data(2 + i, "(none)");
    }
    task_t *task; 
    for (task = runqueue.head; task != NULL; task = task->next) {
        if (task->status == TaskActive &&
                task->process != NULL &&
                task->process->terminal != NULL) {
            set_segment_data(task->process->terminal->index + 2, task->process->program);
        }
    }
    write_status_bar();
}
