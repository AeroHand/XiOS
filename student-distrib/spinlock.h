#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include "types.h"


typedef struct spinlock {
		enum lock_status {
			SpinlockUnlocked = 0,
			SpinlockLocked = 1,
		} dd;
} spinlock_t;

#define SPINLOCK_UNLOCKED {0}

void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

void block_interrupts(uint32_t *flags);
void restore_interrupts(uint32_t flags);

void spin_lock_irqsave(spinlock_t *lock, uint32_t *flags);
void spin_unlock_irqrestore(spinlock_t *lock, uint32_t flags);

#endif
