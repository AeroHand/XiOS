#ifndef _EFS_H
#define _EFS_H

#include "types.h"

#define NAME_MAX 32

#define DENTRY_RTC 0
#define DENTRY_DIRECTORY 1
#define DENTRY_FILE 2

typedef struct block {
    uint8_t reserved[1024];
} block_t;

typedef struct super_block {
    uint32_t num_blocks;
    uint8_t block_map[4092];
} super_block_t;

typedef struct master_entry {
    uint32_t num_dentries;
    uint32_t num_inodes;
    uint32_t num_data_blocks;
    uint8_t reserved[48];
} master_entry_t;

typedef struct inode {
    uint32_t length;
    uint32_t data_blocks[1023];
} inode_t;

typedef struct data_block {
    uint8_t data[1024];
} data_block_t;

typedef struct dentry {
    uint8_t name[NAME_MAX];
    uint32_t type;
    uint32_t block_index;
    uint8_t reserved[24];
} dentry_t;

typedef struct dentry_block {
    master_entry_t master_entry;
    dentry_t dentry[63];
} dentry_block_t;

void efs_set_start(void* address);
int32_t efs_mkdir(uint32_t parent_index);
int32_t efs_read_dentry_by_index(dentry_block_t* dentry_block, uint32_t index,
		dentry_t* dentry);
int32_t efs_read_dentry_by_name(dentry_block_t* dentry_block, const uint8_t*
		fname, dentry_t* dentry);

#endif
