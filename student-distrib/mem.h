#ifndef __MEM_H
#define __MEM_H

#include "lib.h"

#define STORAGE_BYTES MB(24)

void *kmalloc(uint32_t size);
void kfree(void *ptr);
void init_mem();

#endif
