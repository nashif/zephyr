/*
 * Copyright (c) 2019 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <wait_q.h>
#include <ksched.h>
#include <sys/printk.h>

static struct k_spinlock lock;

void z_impl_k_eventflag_init(struct k_eventflag *ev_flag)
{
	z_waitq_init(&ev_flag->wait_q);
	ev_flag->flags = 0U;
	z_object_init(ev_flag);

}

void z_impl_k_eventflag_set(struct k_eventflag *ev_flag, u32_t flags)
{
	k_spinlock_key_t key = k_spin_lock(&lock);

	struct k_thread *thread = z_unpend_first_thread(&ev_flag->wait_q);
	ev_flag->flags |= flags;
	if (thread != NULL) {
		z_ready_thread(thread);
		arch_thread_return_value_set(thread, 0);
	}

	k_spin_unlock(&lock, key);
}


u32_t z_impl_k_eventflag_get(struct k_eventflag *ev_flag)
{
	return ev_flag->flags;
}

void z_impl_k_eventflag_clear(struct k_eventflag *ev_flag, u32_t flags)
{
	if (flags == 0U) {
		flags = 0x7fffffff;
	}
	ev_flag->flags &= ~(flags);
}


u32_t z_impl_k_eventflag_wait(struct k_eventflag *ev_flag, u32_t flags,
			      u8_t options, s32_t timeout)
{
	int ret;

	k_spinlock_key_t key = k_spin_lock(&lock);

	if (options == K_EVENTFLAGS_OR || options == K_EVENTFLAGS_OR_CLEAR) {

		if ((ev_flag->flags & flags) != 0 ) {
			//printk("one bit only\n");
			if (options == K_EVENTFLAGS_OR_CLEAR) {
				ev_flag->flags &= ~(flags);
			}
			k_spin_unlock(&lock, key);
			return ev_flag->flags;
		}
	} else if (options == K_EVENTFLAGS_AND || options == K_EVENTFLAGS_AND_CLEAR) {
		// printk("flags requested: 0x%x\n", flags);
		//printk("flags available: 0x%x\n", ev_flag->flags);
		if ((ev_flag->flags & flags) == flags ) {
			//printk("here\n");
			if (options == K_EVENTFLAGS_AND_CLEAR) {
				ev_flag->flags &= ~(flags);
			}
			k_spin_unlock(&lock, key);
			return ev_flag->flags;
		} else {
			//printk("no match, sorry\n");
		}
	}
	//printk("blocking\n");
	ret = z_pend_curr(&lock, key, &ev_flag->wait_q, timeout);
	return ret;
}
