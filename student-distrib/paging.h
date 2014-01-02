/**
 * vim: tw=80:ts=4:sw=4:et
 * paging.h
 *
 * Functions to manipulate page directories and page tables, as well as the
 * relevant system registers.
 */

#ifndef _PAGING_H
#define _PAGING_H

/**
 * Enum to make privilege levels human-readable.
 */
typedef enum privilege_t { 
    KernelPrivilege = 0, 
    UserPrivilege = 3 
} privilege_t;

/**
 * Struct to hold the physical page directories and page tables.
 */
typedef struct page_data_t {
    page_dir_entry_t pd[1024] __attribute__((aligned(0x1000)));
    page_table_entry_t pt[3][1024] __attribute__((aligned(0x1000)));
} page_data_t;

/**
 * (unused) struct to hold the physical page directories and page tables, along with process ID information.
 */
typedef struct page_info_t{
    uint32_t pid;
    page_data_t	data;
} page_info_t;

// Functions to manipulate paging.  Designed to be as abstract as possible (especially the mapping functions).
// The functions only require the minimum amount of information to define a paging mapping.
void init_paging(void);
void load_pages(uint32_t pid);
void map_4mb_page(uint32_t p_addr, uint32_t v_addr, uint32_t pid, privilege_t privilege);
void map_4kb_page(uint32_t p_addr, uint32_t v_addr, uint32_t pid, privilege_t privilege, uint32_t ptid);
void enable_paging(void);
void clear_page_table(uint32_t pid, uint32_t ptid);
void remap_4kb_page(uint32_t new_p_addr, uint32_t new_v_addr, uint32_t pid, privilege_t new_privilege, uint32_t ptid);
void copy_4kb_page(void* dest, void* src);

#endif /* _PAGING_H */
