#include "efs.h"
#include "lib.h"

block_t* efs_blocks;

void efs_set_start(void* address) {
    efs_blocks = (block_t*)address;
    return;
}

int32_t efs_get_new_block(void);
int32_t efs_num_data_blocks(void);

void efs_new(void) {
    uint32_t i;
    super_block_t* super_block;
    super_block = (super_block_t*) &efs_blocks[0];
    super_block->block_map[0] = 1;
    for(i = 1; i < 4092; i++) {
        super_block->block_map[i] = 0;
    }
    super_block->num_blocks = 1;
    efs_mkdir(1);
    return;
}

int32_t efs_mkdir(uint32_t parent_index) {
    int32_t dentry_block_index;
    dentry_block_t* dentry_block;
    dentry_block_index = efs_get_new_block();
    dentry_block = (dentry_block_t*) &efs_blocks[dentry_block_index];
    dentry_block->master_entry.num_dentries = 2;
    dentry_block->master_entry.num_inodes = 0;
    dentry_block->master_entry.num_data_blocks = 0;
    strcpy((int8_t*)dentry_block->dentry[0].name, ".");
    dentry_block->dentry[0].type = DENTRY_DIRECTORY;
    dentry_block->dentry[0].block_index = dentry_block_index;
    strcpy((int8_t*)dentry_block->dentry[1].name, "..");
    dentry_block->dentry[1].type = DENTRY_DIRECTORY;
    dentry_block->dentry[1].block_index = parent_index;
    return 0;
}

int32_t efs_read_dentry_by_index(dentry_block_t* dentry_block,
                             uint32_t index, dentry_t* dentry) {
    if(index >= dentry_block->master_entry.num_dentries) {
        return -1;
    }
    *dentry = dentry_block->dentry[index];
    return 0;
}

int32_t efs_read_dentry_by_name(dentry_block_t* dentry_block,
                            const uint8_t* fname, dentry_t* dentry) {
    dentry_t tmp_dentry;
    uint32_t num_dentries = dentry_block->master_entry.num_dentries;
    uint32_t i;
    uint32_t bucket;

    //check for valid file name size
    bucket = strlen((int8_t*)fname);
    if(bucket < 1 || bucket > NAME_MAX) {
        return -1;
    }

    //check if file with fname exists
    for(i = 0; i < num_dentries; i++) {
        bucket = efs_read_dentry_by_index(dentry_block, i, &tmp_dentry);
        if(bucket < 0) {
            return -1;
        }
        if(strncmp((int8_t*)fname, (int8_t*)tmp_dentry.name, NAME_MAX) == 0) {
            *dentry = tmp_dentry;
            return 0;
        }
    }
    return -1;
}

/* Read data
 * Reads (length) bytes from the file with inode index (inode) starting from (offset)
 *   bytes. The data is copied into (buf), a string pointer.
 *
 * Returns the number of bytes read, -1 on failure
 */
int32_t efs_read_data(void* inode, uint32_t offset, uint8_t* buf, int32_t length)
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
        if(inode_ptr->data_blocks[cur_block] >= efs_num_data_blocks())
        {
            return -1;
        }
        byte_ptr = (uint8_t*) &efs_blocks[inode_ptr->data_blocks[cur_block]]
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

/* Write data
 * Writes (length) bytes from the file with inode pointer (inode) starting
 *   from (offset) bytes. The data is copied into (buf), a string pointer.
 *
 * Returns the number of bytes written, -1 on failure
 */
int32_t efs_write_data(void* inode, uint32_t offset, uint8_t* buf, int32_t length)
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
    // if offset is longer than file, then there is nothing to write
    // (better to get this out of the way, to remove 'negative' cases)
    if(offset > file_length)
    {
        return 0;
    }
    // update length to represent actual amount of bytes to write
    if(length > file_length - offset)
    {
        length = file_length - offset;
    }
    // length- amount of bytes left to write
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
        if(inode_ptr->data_blocks[cur_block] >= efs_num_data_blocks())
        {
            return -1;
        }
        byte_ptr = (uint8_t*) &efs_blocks[inode_ptr->data_blocks[cur_block]]
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

int32_t efs_get_new_block(void) {
    super_block_t* super_block;
    uint32_t i;
    uint8_t* block_map;
    super_block = (super_block_t*) &efs_blocks[0];
    block_map = super_block->block_map;
    for(i = 1; i < 4092; i++) {
        if(block_map[i] == 0) {
            break;
        }
    }
    if(i > 4092) {
        return -1;
    }
    block_map[i] = 1;
    super_block->num_blocks++;
    return i;
}

dentry_block_t* efs_get_root_dentry_block(void) {
    return (dentry_block_t*) &efs_blocks[1];
}

int32_t efs_num_data_blocks() {
	return 0;
}
