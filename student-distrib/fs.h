/* fs.h - Defines functions and structures of the filesystem for ece391
 * vim:ts=4:sw=4:et
 */

#ifndef __FS_H
#define __FS_H

#include "types.h"

#define NAME_MAX 32

typedef struct master_entry {
    uint32_t num_dentries;
    uint32_t num_inodes;
    uint32_t num_data_blocks;
    uint8_t reserved[52];
} master_entry_t;

typedef struct dentry {
    uint8_t name[NAME_MAX];
    uint32_t type;
    uint32_t inode;
    uint8_t reserved[24];
} dentry_t;

typedef struct inode {
    uint32_t length;
    uint32_t data_blocks[1023];
} inode_t;

typedef struct data_block {
    uint8_t data[4096];
} data_block_t;

typedef struct bootblock {
    master_entry_t master_entry;
    dentry_t dentry[63];
} bootblock_t;


enum file_type {
    FileRTC = 0,
    FileTerminal = 1,
    FileRegular = 2,
    FileDirectory = 3,
};

#define DENTRY_RTC 0
#define DENTRY_DIRECTORY 1
#define DENTRY_FILE 2

struct file_info;

typedef struct file_ops {
    int32_t (*read_func)(struct file_info *, uint8_t*, int32_t);
    int32_t (*write_func)(struct file_info *, const int8_t*, int32_t);
    int32_t (*open_func)(void);
    int32_t (*close_func)(struct file_info *);
} file_ops_t;

typedef struct file_info {
    struct file_ops *file_ops;
    inode_t* inode_ptr;
    // offset into the file
    uint32_t pos;
    union {
        uint32_t flags;
        struct {
            uint32_t _reserved : 21;
            enum file_type type : 8;
            uint32_t can_write : 1;
            uint32_t can_read : 1;
            uint32_t in_use : 1;
        } __attribute__((packed));
    };
} file_info_t;


int32_t read_dentry_by_name(const uint8_t* fname, dentry_t* dentry);
int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry);
int32_t read_data(void* inode, uint32_t offset, uint8_t* buf, int32_t length);
int32_t read_directory_index(int32_t filenum, uint8_t* buf, int32_t length);
int32_t file_read(file_info_t *file, uint8_t *buf, int32_t length);
int32_t directory_read(file_info_t *file, uint8_t* buf, int32_t length);
int32_t get_executables(char** dir, int32_t num_files);
void set_fs_start(uint32_t addr);
inode_t * get_inode_ptr(uint32_t inode);
int32_t fs_open(void);
int32_t fs_close(file_info_t *file);
int32_t fs_write(file_info_t*, const int8_t*, int32_t);
int32_t get_inode_map(uint32_t* inode_map, uint32_t size);
int32_t get_inode_data_block_map(uint32_t index, uint32_t* db_map, uint32_t size);
int32_t get_data_block_map(uint32_t* db_map, uint32_t size);

#endif /* __FS_H */
