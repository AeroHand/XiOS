/* File System
 * vim:ts=4:sw=4:et
 */

#include "lib.h"
#include "fs.h"
#include "mem.h"

uint32_t get_num_dentries(void);
uint32_t get_num_inodes(void);
uint32_t get_num_data_blocks(void);
master_entry_t* get_master_entry_addr(void);

uint32_t fs_start;

static inode_t *inodes;
static dentry_t *dentries;
static data_block_t *data_blocks;

/**
 * Set file system starting address
 * Used for setting the start of the file system in memory.
 * This exists because we load the fs as a module in GRUB.
 */
void set_fs_start(uint32_t addr)
{
    fs_start = addr;
    inodes = (inode_t*) (fs_start + sizeof(bootblock_t));
    dentries = (dentry_t*) (fs_start + sizeof(master_entry_t));
    data_blocks = (data_block_t*) (fs_start + sizeof(bootblock_t) +
            get_num_inodes() * sizeof(inode_t));
    return;
}

inode_t *get_inode_ptr(uint32_t inode) {
    return inodes + inode;
}

/**
 * File system open
 * This will be later used for getting file discriptors.
 */
int32_t fs_open(void)
{
    return -1;
}

/**
 * File system write
 * THIS SHOULD NOT BE CALLED. Read only filesystem.
 */
int32_t fs_write(file_info_t *file, const int8_t* buf, int32_t nbytes)
{
    return -1;
}

/**
 * Read dentry by name
 * Takes a file name (fname) and finds the dentry with that name.
 *   The dentry is copied into the dentry passed by pointer.
 *
 * Returns 0 on success, -1 on failure
 */
int32_t read_dentry_by_name(const uint8_t* fname, dentry_t* dentry)
{
    dentry_t tmp_dentry;
    uint32_t num_dentries = get_num_dentries();
    uint32_t i;
    uint32_t bucket;
    
    //check for valid file name size
    bucket = strlen((int8_t*)fname);
    if(bucket < 1 || bucket > NAME_MAX)
    {
        return -1;
    }

    //check if file with fname exists
    for(i = 0; i < num_dentries; i++)
    {
        bucket = read_dentry_by_index(i, &tmp_dentry);
        if(strncmp((int8_t*)fname, (int8_t*)tmp_dentry.name, NAME_MAX) == 0)
        {
            *dentry = tmp_dentry;
            return 0;
        }
    }
    return -1;
}

/**
 * Read dentry by index
 * Takes an dentry index and copies the dentry into the dentry passed by pointer.
 *
 * Returns 0 on success, -1 on failure
 */
int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry)
{
    if(index >= get_num_dentries())
    {
        return -1;
    }
    *dentry = dentries[index];
    return 0;
}

/**
 * Read data
 * Reads (length) bytes from the file with inode index (inode) starting from (offset)
 *   bytes. The data is copied into (buf), a string pointer.
 *
 * Returns the number of bytes read, -1 on failure
 */
int32_t read_data(void* inode, uint32_t offset, uint8_t* buf, int32_t length)
{
    inode_t* inode_ptr = (inode_t*) inode;
    uint8_t* byte_ptr;
    uint32_t file_length;
    uint32_t copied_length = 0;
    uint32_t cur_block;
    uint32_t bytes_left_in_block;
    uint32_t n;
    file_length = inode_ptr->length;
    // check that file isn't too long to be stored in the inode's data_blocks
    if (file_length / 4096 > 1023)
    {
        return -1;
    }
    // if offset is longer than file, then there is nothing to read
    // (better to get this out of the way, to remove 'negative' cases)
    if(offset > file_length)
    {
        return 0;
    }
    // update length to represent actual amount of bytes to read
    if(length > file_length - offset)
    {
        length = file_length - offset;
    }
    // length- amount of bytes left to read
    // offset- current position in file
    // cur_block- current block that offset is within
    // byte_ptr- pointer to the first byte to copy in this iteration
    // bytes_left_in_block- how many bytes are left in current block
    // n- number of bytes to copy in this iteration
    while(length > 0)
    {
        cur_block = offset / 4096;
        // check if current block is within the valid range for the
        //   whole file system. if not, there is a serious problem.
        if(inode_ptr->data_blocks[cur_block] >= get_num_data_blocks())
        {
            return -1;
        }
        byte_ptr = (uint8_t*) &data_blocks[inode_ptr->data_blocks[cur_block]]
            + (offset % 4096);
        bytes_left_in_block = 4096 - (offset % 4096);
        // if there are more bytes to copy than there are bytes left in the
        //   block, than just copy what's left in the block.
        // else copy remainder of (length)
        n = (length > bytes_left_in_block)?bytes_left_in_block:length;
        memcpy(buf, byte_ptr, n); 
        // update the amount of bytes left to copy
        length -= n;
        // update the total amount of bytes copied
        copied_length += n;
        // update the position of the buffer (which is copied to)
        buf += n;
        // update the current offset into the file
        offset += n;
    }
    return copied_length;
}

int32_t file_read(file_info_t *file, uint8_t *buf, int32_t length) {
    int32_t bytes_read = read_data(file->inode_ptr, file->pos, buf, length);
    file->pos += bytes_read;
    return bytes_read;
}

/**
 * read a filename by index from the directory
 */
int32_t read_directory_index(int32_t filenum, uint8_t* buf, int32_t length) {
    uint32_t num_dentries = get_num_dentries();
    // can't read any more entries
    if (filenum >= num_dentries) {
        return 0;
    }
    int32_t i;
    for (i = 0; dentries[filenum].name[i]; i++) {
        buf[i] = dentries[filenum].name[i];
    }
    int32_t bytes_read = i;
    while (i < length) {
        buf[i++] = '\0';
    }
    return bytes_read;
}

/**
 * read system call for the directory
 *
 * uses the file position integer to keep track of which index to use
 */
int32_t directory_read(file_info_t *file, uint8_t* buf, int32_t length)
{
    int32_t bytes_read = read_directory_index(file->pos, buf, length);
    file->pos++;
    return bytes_read;
}

/**
 * get a list of executables in the directory
 *
 * allocates memory which the caller should free
 *
 * @param dir an array of pointers which will be set to the file names
 * @num_files maximum number of files (ie, the size of dir)
 * @return the actual number of files in the system
 */
int32_t get_executables(char** dir, int32_t num_files) {
    int filenum;
    int32_t bytes_read;
    for (filenum = 0; filenum < num_files; filenum++) {
        dir[filenum] = kmalloc(NAME_MAX);
        if (dir[filenum] == NULL) {
            filenum--;
            break;
        }
        bytes_read = read_directory_index(filenum, (uint8_t*) dir[filenum], NAME_MAX);
        if (bytes_read == 0) {
            kfree(dir[filenum]);
            dir[filenum] = NULL;
            break;
        }
    }
    return filenum;
}

int32_t fs_close(file_info_t *file)
{
    return 0;
}

/**
 * Get master entry addr
 * Finds the address of the master entry.
 *
 * Returns pointer to the master entry
 */
master_entry_t* get_master_entry_addr(void)
{
    return (master_entry_t*)fs_start;
}

/**
 * Get number of dentries
 * Finds the number of dentries in the file system.
 *
 * Returns number of dentries
 */
uint32_t get_num_dentries(void)
{
    master_entry_t* bblock = get_master_entry_addr();
    return bblock->num_dentries;
}

/**
 * Get number of inodes
 * Finds the number of inodes in the file system.
 *
 * Returns the number of inodes
 */
uint32_t get_num_inodes(void)
{
    master_entry_t* bblock = get_master_entry_addr();
    return bblock->num_inodes;
}

/**
 * Get number of data blocks
 * Finds the number of data blocks in the file system.
 *
 * Returns number of data blocks
 */
uint32_t get_num_data_blocks(void)
{
    master_entry_t* bblock = get_master_entry_addr();
    return bblock->num_data_blocks;
}

int32_t get_inode_map(uint32_t* inode_map, uint32_t size) {
    uint32_t num_dentries = get_num_dentries();
    dentry_t dentry;
    uint32_t i;
    if(num_dentries > size) {
        return -1;
    }
    for(i = 0; i < num_dentries; i++) {
        read_dentry_by_index(i, &dentry);
        inode_map[i] = dentry.inode;
    }
    return (int32_t)num_dentries;
}

int32_t get_inode_data_block_map(uint32_t index, uint32_t* db_map, uint32_t size) {
    uint32_t length, num_blocks, i;
    dentry_t dentry;
    inode_t* inode_ptr;
    read_dentry_by_index(index, &dentry);
    inode_ptr = get_inode_ptr(dentry.inode);
    length = inode_ptr->length;
    num_blocks = length / 4096 + 1;
    if(num_blocks > size) {
        return -1;
    }
    for(i = 0; i < num_blocks; i++) {
        db_map[i] = ((uint32_t*)inode_ptr)[i];
    }
    return (int32_t)num_blocks;
}

int32_t get_data_block_map(uint32_t* db_map, uint32_t size) {
    uint32_t num_blocks, num_dentries, inode_num_blocks, i, j, k;
    uint32_t inode_db_map[10];
    num_blocks = get_num_data_blocks();
    if(num_blocks > size) {
        return -1;
    }
    num_dentries = get_num_dentries();
    k = 0;
    for(i = 0; i < num_dentries; i++) {
        inode_num_blocks = get_inode_data_block_map(i, inode_db_map, 10);
        if(inode_num_blocks < 0) {
            return -1;
        }
        for(j = 0; j < inode_num_blocks; j++) {
            db_map[k] = inode_db_map[j];
            k++;
        }
    }
    return (int32_t)k;
}

#if 0
int32_t compress_data_blocks(uint32_t* db_map) {
    uint32_t inode_map[24];
    uint32_t map[24][5];
    uint32_t current_db, found, i, j, k;
    int32_t num_dentries, num_db;
    num_dentries = get_inode_map(inode_map, 24);
    if(num_dentries < 0) {
        return -1;
    }
    for(i = 0; i < num_dentries; i++) {
        num_db = get_inode_data_block_map(i, map[i], 5);
    num_db = get_data_block_map(db_map, 100);
    num_dentries = get_num_dentries();
    if(num_dentries < 
    if(num_db < 0) {
        return -1;
    }
    current_db = -1;
    for(i = 0; i < num_db; i++) {
        found = 0;
        while(found == 0) {
            current_db++;
            for(j = 0; j < num_db; j++) {
                if(current_db == db_map[j]) {
                    found = 1;
                    break;
                }
            }
        }
        memcpy(data_blocks[current_db], data_blocks[
#endif
