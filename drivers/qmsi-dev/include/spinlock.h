/*
 * {% copyright %}
 */

#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "qm_soc_regs.h"
/*
 * Single, shared spinlock which can be used for synchronization between the
 * Lakemont and ARC cores.
 * The Spinlock lock size and position in RAM must be same on both cores.
 */
#if (QUARK_SE)

typedef struct {
	volatile char flag[2];
	volatile char turn;
} spinlock_t;

extern spinlock_t __esram_lock_start;
void spinlock_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);

#define QM_SPINLOCK_LOCK() spinlock_lock(&__esram_lock_start)
#define QM_SPINLOCK_UNLOCK() spinlock_unlock(&__esram_lock_start)

#else

#define QM_SPINLOCK_LOCK()
#define QM_SPINLOCK_UNLOCK()

#endif /* defined(QM_QUARK_SE) */

#endif /* __SPINLOCK_H__ */
