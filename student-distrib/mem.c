#include "mem.h"
/**
 * @file mem.c
 *
 * @brief kernel memory allocator (kmalloc/kfree)
 */

// This limits how much fragmentation is allowed
#define MAX_REGIONS 500

// represents a region of memory in a linked list
typedef struct region {
    uint8_t *ptr;
    uint32_t size;
    struct region *next;
    struct region *prev;
		int8_t in_use;
} region_t;

// static backing storage where memory is doled out from
static uint8_t *storage = (uint8_t*) MB(192);

static region_t regions[MAX_REGIONS];

static region_t* free_regions;
static region_t* allocated_regions;

// Forward declarations
region_t* add_region(region_t* new, region_t* list);
region_t* new_region(void *ptr, uint32_t size);
void remove(region_t *region);
void* ltrim(region_t *region, uint32_t desired_size);
int8_t are_adjacent(region_t *first, region_t *second);
int8_t comp(void *left, void *right);
int8_t in_region(void *ptr, region_t *region);

/**
 * allocate a block of memory and reserve it
 *
 * @param size number of bytes to allocate
 * @return pointer to the allocated memory or NULL of no sufficiently large
 * free regions were found
 */
void * kmalloc(uint32_t size) {
	if (size == 0) {
		return NULL;
	}
	region_t *region = free_regions;
	while (region != NULL) {
		if (are_adjacent(region, region->next)) {
			region->size += region->next->size;
			remove(region->next);
		}
		if (region->size >= size) {
			void *ptr = ltrim(region,size);
			region_t *new = new_region(ptr, size);
			add_region(new, allocated_regions);
			return ptr;
		}
		region = region->next;
	}

	return NULL;
}

/**
 * free a block of memory previously allocated with kmalloc
 *
 * does nothing when trying to free NULL
 * does nothing if ptr was not allocated (including free'ing pointers in the
 * middle of a block)
 *
 * @param ptr pointer returned by a previous kmalloc
 */
void kfree(void *ptr) {
	if (ptr == NULL) {
		return;
	}
	region_t *region = allocated_regions;
	while (region != NULL) {
		if (region->ptr == ptr) {
			remove(region);
			add_region(region, free_regions);
			region->in_use = 1;
			return;
		}

		region = region->next;
	}
}

/**
 * add a region to a linked list
 *
 * keeps list sorted by ptr
 *
 * @param new pointer to new region, already initialized
 * @param list head of a list to insert into
 * @return the region pointer passed in (for chaining)
 */
region_t* add_region(region_t* new, region_t* list) {
	if (list == NULL) {
		return NULL;
	}
	region_t* region = list;
	region_t* prev_region = NULL;
	while (region != NULL && comp(region->ptr, new->ptr)) {
		prev_region = region;
		region = region->next;
	}
	if (prev_region != NULL) {
		prev_region->next = new;
	}
	new->prev = prev_region;
	new->next = region;
	if (region != NULL) {
		region->prev = new;
	}

	return new;
}

/**
 * get a new region and initialize it from a pointer and size
 *
 * regions are retrieved from a static pile of them available to this function
 * 
 * @param ptr pointer this region refers to
 * @param size number of bytes this region covers
 * @return the new region
 */
region_t* new_region(void *ptr, uint32_t size) {
	int i;
	for (i = 0; i < MAX_REGIONS; i++) {
		if (regions[i].in_use == 0) {
			regions[i].ptr = ptr;
			regions[i].size = size;
			regions[i].next = NULL;
			regions[i].prev = NULL;
			regions[i].in_use = 1;
			return &regions[i];
		}
	}
	return NULL;
}

/**
 * reduce the size of a region from the left
 *
 * makes a region smaller and returns a pointer to a block with a desired size
 * available will fail (and return NULL) if that region doesn't actually have
 * enough space
 *
 * @param region the region to get space from
 * @param desired_size the number of bytes to trim from the left
 * @return a pointer to a block of desired_size bytes
 */
void* ltrim(region_t *region, uint32_t desired_size) {
	if (region->size < desired_size) {
		return NULL;
	}
	void *oldptr = region->ptr;
	if (region->size == desired_size) {
		// need to claim entire region
		remove(region);
	} else {
		// just reduce this region and give back a ptr for only part of it
		region->size -= desired_size;
		region->ptr += desired_size;
	}
	return oldptr;
}

/**
 * checks if two regions are adjacent
 *
 * specifically reports if the first region ends at the start of the second region
 *
 * @return 0 if regions are not adjacent or either is invalid, 1 if regions are
 * adjacent
 */
int8_t are_adjacent(region_t* first, region_t* second) {
	if (first == NULL || second == NULL) {
		return 0;
	}
	if (first->ptr + first->size == second->ptr) {
		return 1;
	}
	return 0;
}

/**
 * remove a region from its list
 *
 * also marks the region free so that it can be re-used when memory is further fragmented
 *
 * clears out the storage memory so that allocated memory is always clean
 *
 * @param region the region to free
 */
void remove(region_t *region) {
	region->prev->next = region->next;
	if (region->next != NULL) {
		region->next->prev = region->prev;
	}
	memset(region->ptr, 0, region->size);
	region->in_use = 0;
}

/**
 * helper to compare pointers
 *
 * returns true if left pointer comes before right
 */
int8_t comp(void *left, void *right) {
	return (uint32_t) left < (uint32_t) right;
}

/**
 * helper to check if a pointer is in a region
 */
int8_t in_region(void *ptr, region_t *region) {
	return (comp(region->ptr, ptr) && comp(ptr, region->ptr + region->size));
}

/**
 * initialize the memory system
 *
 * sets up a sentinel for the free and allocated region lists and clears the
 * storage memory
 */
void init_mem() {
	// sentinel for free regions
	regions[0].ptr = NULL;
	regions[0].size = 0;
	regions[0].next = &regions[1];
	regions[0].prev = NULL;
	regions[0].in_use = 1;

	regions[1].ptr = storage;
	regions[1].size = STORAGE_BYTES;
	regions[1].next = NULL;
	regions[1].prev = &regions[0];
	regions[1].in_use = 1;

	free_regions = &regions[0];

	// sentinel for allocated regions
	regions[2].ptr = NULL;
	regions[2].size = 0;
	regions[2].next = NULL;
	regions[2].prev = NULL;
	regions[2].in_use = 1;
	allocated_regions = &regions[2];

	memset(storage, 0, STORAGE_BYTES);
}

