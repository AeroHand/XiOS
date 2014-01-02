/* kernel.c - the C part of the kernel
 * vim:ts=4:sw=4:et
 */

#include "multiboot.h"
#include "x86_desc.h"
#include "lib.h"
#include "i8259.h"
#include "fs.h"
#include "task.h"
#include "debug.h"
#include "paging.h"
#include "interrupt.h"
#include "syscall.h"
#include "fs.h"
#include "rtc.h"
#include "shutdown.h"
#include "mem.h"
#include "pit.h"
#include "keyboard.h"
#include "mouse.h"
#include "status.h"
#include "sb16.h"
#include "fdc.h"

/* Macros. */
/* Check if the bit BIT in FLAGS is set. */
#define CHECK_FLAG(flags,bit)   ((flags) & (1 << (bit)))

#define DEBUG_FS 0

void left_click(int32_t x, int32_t y) {
    printf("clicked at %d, %d\n", x, y);
}

/* Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR. */
    void
entry (unsigned long magic, unsigned long addr)
{
    multiboot_info_t *mbi;

    init_mem();
    init_paging();
    enable_paging();
	init_mouse();
    init_terminals();
    init_status();
    i8259_init();
    rtc_init();
    init_processes();

    /* Setup VGA settings for terminal */
    init_graphics();

    /* Am I booted by a Multiboot-compliant boot loader? */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        printf ("Invalid magic number: 0x%#x\n", (unsigned) magic);
        return;
    }

    /* Set MBI to the address of the Multiboot information structure. */
    mbi = (multiboot_info_t *) addr;

    /* Print out the flags. */
    printf ("flags = 0x%#x\n", (unsigned) mbi->flags);

    /* Are mem_* valid? */
    if (CHECK_FLAG (mbi->flags, 0))
        printf ("mem_lower = %uKB, mem_upper = %uKB\n",
                (unsigned) mbi->mem_lower, (unsigned) mbi->mem_upper);

    /* Is boot_device valid? */
    if (CHECK_FLAG (mbi->flags, 1))
        printf ("boot_device = 0x%#x\n", (unsigned) mbi->boot_device);

    /* Is the command line passed? */
    if (CHECK_FLAG (mbi->flags, 2))
        printf ("cmdline = %s\n", (char *) mbi->cmdline);

    if (CHECK_FLAG (mbi->flags, 3)) {
        int mod_count = 0;
        int i;
        module_t* mod = (module_t*)mbi->mods_addr;
        while(mod_count < mbi->mods_count) {
            printf("Module %d loaded at address: 0x%#x\n", mod_count, (unsigned int)mod->mod_start);
            printf("Module %d ends at address: 0x%#x\n", mod_count, (unsigned int)mod->mod_end);
            printf("First few bytes of module:\n");
            for(i = 0; i<16; i++) {
                printf("0x%x ", *((char*)(mod->mod_start+i)));
            }
            printf("\n");
            mod_count++;
        }
    }
    /* Bits 4 and 5 are mutually exclusive! */
    if (CHECK_FLAG (mbi->flags, 4) && CHECK_FLAG (mbi->flags, 5))
    {
        printf ("Both bits 4 and 5 are set.\n");
        return;
    }

    /* Is the section header table of ELF valid? */
    if (CHECK_FLAG (mbi->flags, 5))
    {
        elf_section_header_table_t *elf_sec = &(mbi->elf_sec);

        printf ("elf_sec: num = %u, size = 0x%#x,"
                " addr = 0x%#x, shndx = 0x%#x\n",
                (unsigned) elf_sec->num, (unsigned) elf_sec->size,
                (unsigned) elf_sec->addr, (unsigned) elf_sec->shndx);
    }

    /* Are mmap_* valid? */
    if (CHECK_FLAG (mbi->flags, 6))
    {
        memory_map_t *mmap;

        printf ("mmap_addr = 0x%#x, mmap_length = 0x%x\n",
                (unsigned) mbi->mmap_addr, (unsigned) mbi->mmap_length);
        for (mmap = (memory_map_t *) mbi->mmap_addr;
                (unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
                mmap = (memory_map_t *) ((unsigned long) mmap
                    + mmap->size + sizeof (mmap->size)))
            printf (" size = 0x%x,     base_addr = 0x%#x%#x\n"
                    "     type = 0x%x,  length    = 0x%#x%#x\n",
                    (unsigned) mmap->size,
                    (unsigned) mmap->base_addr_high,
                    (unsigned) mmap->base_addr_low,
                    (unsigned) mmap->type,
                    (unsigned) mmap->length_high,
                    (unsigned) mmap->length_low);
    }

    /* Construct an LDT entry in the GDT */
    {
        seg_desc_t the_ldt_desc;
        the_ldt_desc.granularity    = 0;
        the_ldt_desc.opsize         = 1;
        the_ldt_desc.reserved       = 0;
        the_ldt_desc.avail          = 0;
        the_ldt_desc.present        = 1;
        the_ldt_desc.dpl            = 0x0;
        the_ldt_desc.sys            = 0;
        the_ldt_desc.type           = 0x2;

        SET_LDT_PARAMS(the_ldt_desc, &ldt, ldt_size);
        ldt_desc_ptr = the_ldt_desc;
        lldt(KERNEL_LDT);
    }

    /* Construct a TSS entry in the GDT */
    {
        seg_desc_t the_tss_desc;
        the_tss_desc.granularity    = 0;
        the_tss_desc.opsize         = 0;
        the_tss_desc.reserved       = 0;
        the_tss_desc.avail          = 0;
        the_tss_desc.seg_lim_19_16  = TSS_SIZE & 0x000F0000;
        the_tss_desc.present        = 1;
        the_tss_desc.dpl            = 0x0;
        the_tss_desc.sys            = 0;
        the_tss_desc.type           = 0x9;
        the_tss_desc.seg_lim_15_00  = TSS_SIZE & 0x0000FFFF;

        SET_TSS_PARAMS(the_tss_desc, &tss, tss_size);

        tss_desc_ptr = the_tss_desc;

        tss.ldt_segment_selector = KERNEL_LDT;
        tss.ss0 = KERNEL_DS;
        tss.esp0 = MB(8);
        ltr(KERNEL_TSS);
    }

    /* Make room for the filesystem 'RAM disk' */
    uint8_t *ram_disk = kmalloc(FDC_MAX_SIZE);

    init_interrupts();
    // the kernel process should not be active
    idle_task(current_process->task);

    /* Load the filesystem into the RAM disk */
    // disable scheduling (PIT interrupt)
    disable_irq(0);
    // disable keyboard interrupts
    disable_irq(1);
    sti();
    int32_t fdc_error;
    fdc_error = fdc_init(0);
    //if(fdc_write(moyd->mod_start, FDC_MAX_SIZE) == 0) {
    if((fdc_error |= fdc_disk_read(ram_disk, FDC_MAX_SIZE)) == 0) {
        printf("Filesystem loaded into RAM disk\n");
    } else {
        printf("Floppy load error\n");
    }
    cli();
    // re-enable
    enable_irq(0);
    enable_irq(1);

    set_fs_start((uint32_t)ram_disk);

    clear();

#if DEBUG_FS
    uint32_t inode_map[30];
    uint32_t inode_db_map[10];
    uint32_t db_map[100];
    int32_t num_inodes, num_db, i, j;
    num_inodes = get_inode_map(inode_map, 30);
    if(num_inodes < 0) {
        printf("Inode map is too small.\n");
    } else {
        printf("Filesystem mapping (dentry:inode:data blocks):\n");
        for(i = 0; i < num_inodes; i++) {
            printf("%d: %u: ", i, inode_map[i]);
            num_db = get_inode_data_block_map(i, inode_db_map, 10);
            if(num_db < 0) {
                printf("inode_db_map is too small!\n");
            } else {
                for(j = 0; j < num_db; j++) {
                    printf("%u ", inode_db_map[j]);
                }
                printf("\n");
            }
        }
    }
    num_db = get_data_block_map(db_map, 100);
    if(num_db < 0) {
        printf("db_map too small\n");
    } else {
        printf("Data block mapping:\n");
        for(i = 0; i < num_db; i++) {
            printf("%u ", db_map[i]);
        }
    }
#else

    init_sb16();

    //play_wav("startup_hs.wav");

    timer_start(20);

    set_segment_data(0, "Start!");
    set_segment_data(1, "<");

    write_status_bar();

    rtc_init();

    int i;
    for (i = 0; i < 3; i++) {
        kernel_spawn("shell");
    }

    // set the kernel to idle
    idle_task(current_process->task);

    set_status_bar();
    write_status_bar();

    switch_terminals(&terminals[0]);

    sti();
#endif

    /* Spin (nicely, so we don't chew up cycles) */
    asm volatile(".1: hlt; jmp .1;");
}
