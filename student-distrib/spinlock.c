#include "spinlock.h"

/**
 * @file spinlock.c
 *
 * @brief basic spinlock for synchronization
 */

/**
 * lock a spinlock
 *
 * does not return (spins, in fact) until the lock is acquired
 */
void spin_lock(spinlock_t *lock) {
	asm volatile (" \
			spin_lock_loop: \n\
			movl $1, %%eax; \n\
			xchgl %%eax,(%%ebx); \n\
			test %%eax, %%eax; \n\
			jnz spin_lock_loop; \n\
			"
			: /* no ouputs */
			: "b" (&lock->dd)
			: "eax", "cc"
			);
}

/**
 * release a held spinlock
 *
 * assumes you do actually hold the lock; make sure you do call this eventually
 */
void spin_unlock(spinlock_t *lock) {
	lock->dd = 0;
}

/**
 * save interrupt state and disable interrupts
 *
 * @param flags pointer to an int to hold the current state
 */
void block_interrupts(uint32_t *flags) {
	asm volatile (" \
			pushfl; \n\
			popl (%%eax); \n\
			cli; \n\
			"
			: /* no outputs */
			: "a"(flags)
			: "cc"
			);
}
void restore_interrupts(uint32_t flags) {
	asm volatile (" \
			pushl %0; \n\
			popfl; \n\
			"
			: /* no outputs */
			: "r" (flags)
			: "cc"
			);
}

/**
 * lock a spinlock and disable interrupts
 *
 * utterly useless in a uniprocessor setting
 */
void spin_lock_irqsave(spinlock_t *lock, uint32_t* flags) {
	block_interrupts(flags);
	spin_lock(lock);
}

/**
 * unlock a spinlock and restore interrupts
 *
 * utterly useless in a uniprocessor setting
 */
void spin_unlock_irqrestore(spinlock_t *lock, uint32_t flags) {
	spin_unlock(lock);
	restore_interrupts(flags);
}
