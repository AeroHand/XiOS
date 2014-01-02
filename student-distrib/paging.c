// vim:ts=4:sw=4:et
#include "x86_desc.h"
#include "paging.h"
#include "lib.h"
#include "task.h"
#include "mem.h"

/**
 * @file paging.c
 *
 * @brief general-purpose paging infrastructure
 *
 * provides functions for changing page directory and table entries and
 * initialize them for processes.
 */

// Local data structure to hold paging info/addresses used in functions.
static page_data_t page_tables[MAX_PROCESSES] __attribute__((aligned(KB(4))));
static uint32_t asm_address;

// Local functions used for mapping.
void set_pde(uint32_t index, uint32_t address, uint32_t flag, uint32_t pid);
void set_pte(uint32_t index, uint32_t address, uint32_t flags, uint32_t pid, uint32_t pt_index);

// Global variable.
uint32_t page_pid = 0;

/**
 * Function to initialize paging for the system.  Only needs to be called once.
 */
void init_paging() {

    // from osdev guide to paging http://wiki.osdev.org/Paging
    // enable bit 16, write-protect
    // enable bits 4 and 7, pse and pge
    asm volatile ("               \
            movl %%cr0, %%eax   \n\
            orl $0x1000, %%eax  \n\
            movl %%eax, %%cr0   \n\
                                  \
            movl %%cr4, %%eax   \n\
            orl $0x90, %%eax    \n\
            movl %%eax, %%cr4"
            : /* no outputs */
            : /* no inputs */
            : "eax"
            );

    int i, j;

    for(i = 0; i < MAX_PROCESSES; i++)
    {
		// Map the first page table (kernel 0MB-4MB, except for the first 4KB). 
        for(j = 1; j < 1024; j++)
        {
            map_4kb_page(j << 12, j << 12, i, 0, KernelPrivilege);
        }

        //Lets all processes access 4-8MB kernel page.
        map_4mb_page(MB(4), MB(4), i, KernelPrivilege);

        // Map kmalloc region
        int page_num;
        int total_pages = STORAGE_BYTES/MB(4);
        if (STORAGE_BYTES % MB(4) != 0) {
            total_pages++;
        }
        for (page_num = 0; page_num < total_pages; page_num++) {
            map_4mb_page(MB(192 + page_num * 4), MB(192 + page_num * 4), i, KernelPrivilege);
        //    map_4mb_page(MB(192 + page_num * 4), MB(192 + page_num * 4), i, UserPrivilege);
        }
    }

    /** for each process (excluding kernel process case)
     *     - gives kernel permission for current processes physical memory
     *     - gives process permission for current processes physical memory at kernel priv
     *     - gives process mapping to virtual 128MB at user priv
     */
    for(i = 1; i < MAX_PROCESSES; i++)
    {
        map_4mb_page(MB(4 + 4*i), MB(4 + 4*i), 0, KernelPrivilege);
        map_4mb_page(MB(4 + 4*i), MB(4 + 4*i), i, KernelPrivilege);
        map_4mb_page(MB(4 + 4*i), MB(128), i, UserPrivilege);
    }

	// Loads the page tables for process 0 (the kernel).
    load_pages(0);
}

/**
 * Function to switch page tables for a given process.
 * @param pid The process ID whose page tables we want to load.
 */
void load_pages(uint32_t pid)
{
    page_pid = pid;
	
    // Determine the desired page table address.
    asm_address = (uint32_t)&(page_tables[pid].pd);

    // Put the address in CR3.
    asm volatile ("               \
            addl $0x18, %%eax   \n\
            movl %%eax, %%cr3"
            : /* no outputs */
            : "a" (asm_address)
            );
}

/**
 * Function to set a page directory entry.
 * @param index The index of the entry within the page directory.
 * @param address The desired physical memory address to be mapped.
 * @param flags The flags to be set for the page directory entry.
 * @param pid The process ID whose page directory we want to modify.
 */
void set_pde(uint32_t index, uint32_t address, uint32_t flags, uint32_t pid) {
    page_dir_entry_t pde;
    pde.addr = address;
    pde.flags = flags;
    page_tables[pid].pd[index] = pde;
}

/**
 * Function to set a page table entry.
 * @param index The index of the entry within the page directory.
 * @param address The desired physical memory address to be mapped.
 * @param flags The flags to be set for the page directory entry.
 * @param pid The process ID whose page directory we want to modify.
 * @param pt_index The index of the page table that we want to modify. (0 is the 0-4MB kernel page, 1 is used for vidmap)
 */
void set_pte(uint32_t index, uint32_t address, uint32_t flags, uint32_t pid, uint32_t pt_index) {
    page_table_entry_t pte;
    pte.addr = address;
    pte.flags = flags;
    page_tables[pid].pt[pt_index][index] = pte;
}

/**
 * Function to map a 4MB page for a process.
 * @param p_addr The physical address that we want to map.
 * @param v_addr The virtual address that we want to map.
 * @param pid The process ID that we want to map the page in.
 * @param privilege The privelege that we want to use to map the process (user or kernel).
 */
void map_4mb_page(uint32_t p_addr, uint32_t v_addr, uint32_t pid, privilege_t privilege)
{
	// Determine the correct index in the page directory.
    uint32_t index = v_addr / MB(4);
	
	// Set the entry.
    if(privilege == KernelPrivilege)
    {
        set_pde(index, p_addr, 0x09B, pid);
    }
    else
    {
        set_pde(index, p_addr, 0x09F, pid);
    }
}

/**
 * Function to map a 4KB page for a process.
 * @param p_addr The physical address that we want to map.
 * @param v_addr The virtual address that we want to map.
 * @param pid The process ID that we want to map the page in.
 * @param privilege The privelege that we want to use to map the process (user or kernel).
 * @param ptid The index of the page table that we want to modify. (0 is the 0-4MB kernel page, 1 is used for vidmap)
 */
void map_4kb_page(uint32_t p_addr, uint32_t v_addr, uint32_t pid, privilege_t privilege, uint32_t ptid)
{
	// Determine relevant indices.
    uint32_t pt_addr;
    uint32_t pd_index = v_addr / MB(4);
    uint32_t pt_index = (v_addr % MB(4)) / KB(4);

	// Determine the address of the page table.
    pt_addr = (uint32_t)&(page_tables[pid].pt[ptid]);

    // Map the page table in the page directory.
    set_pde(pd_index, pt_addr, 0x1F, pid);

    // Set the relevant page table entry.
    set_pte(pt_index, p_addr, 0x1F, pid, ptid);
}

/**
 * Function to reassign a page table to point to a new address while removing all references to its old address.
 * @param new_p_addr The physical address that we want to map.
 * @param new_v_addr The virtual address that we want to map.
 * @param pid The process ID that we want to map the page in.
 * @param privilege The privelege that we want to use to map the process (user or kernel).
 * @param ptid The index of the page table that we want to modify. (0 is the 0-4MB kernel page, 1 is used for vidmap)
 */
void remap_4kb_page(uint32_t new_p_addr, uint32_t new_v_addr, uint32_t pid, privilege_t new_privilege, uint32_t ptid)
{
    clear_page_table(pid, ptid);
    map_4kb_page(new_p_addr, new_v_addr, pid, new_privilege, ptid);
}

/**
 * Function to map a 4KB page for a process.
 * @param pid The process ID that we want to map the page in.
 * @param ptid The index of the page table that we want to modify. (0 is the 0-4MB kernel page, 1 is used for vidmap)
 */
void clear_page_table(uint32_t pid, uint32_t ptid)
{
    int i;
	
	// For every index in the page directory/table:
    for(i = 0; i < 1024; i++)
    {
        // If a page directory entry points to this page table, just remove it.
        if(page_tables[pid].pd[i].addr_shifted<<12 == (uint32_t)page_tables[pid].pt[ptid])
        {
            page_tables[pid].pd[i].addr = 0;
        }

        // Also, clear the entry in this page table.
        page_tables[pid].pt[ptid][i].addr = 0;
    }
}

/**
 * Function to copy the contents of one 4KB page to another.
 * @param dest The address of the destination page.
 * @param src The address of the source page.
 */
void copy_4kb_page(void* dest, void* src)
{
    memcpy(dest, src, KB(4));
}

/**
 * Function to enable paging systemwide.
 * Only needs to be called once upon boot.
 */
void enable_paging(void) {
    asm volatile ("                   \
            movl %%cr0, %%eax       \n\
            orl $0x80000000, %%eax  \n\
            movl %%eax, %%cr0"
            : /* no outputs */
            : /* no inputs */
            : "eax"
            );
}
