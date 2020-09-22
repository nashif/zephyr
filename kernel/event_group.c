/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <spinlock.h>
#include <wait_q.h>
#include <ksched.h>
#include <logging/log.h>
LOG_MODULE_DECLARE(os);

static struct k_spinlock lock;

struct thread_event {
	sys_dnode_t _node;
	uint32_t flags;
};

#define EVENT_FLAGS_CONTROL_BITS 0xff000000UL
#define EVENT_FLAGS_WAIT_FOR_ALL 0x04000000UL
#define EVENT_FLAGS_CLEAR_BITS 0x01000000UL

static bool check_event_flags(uint32_t current_flags, uint32_t flags_to_match,
			      uint8_t options)
{
	bool match = false;

	if ((options & K_EVGROUP_ALL) != 0U) {
		if ((current_flags & flags_to_match) == flags_to_match) {
			match = true;
		}
	} else {
		if ((current_flags & flags_to_match) != 0) {
			match = true;
		}
	}

	if (match) {
		LOG_INF("eventflag match: 0x%x", flags_to_match);
	} else {
		LOG_INF("eventflag: no match");
	}
	return match;
}

static uint32_t set_control_bits(uint32_t options)
{
	uint32_t control_bits = 0;

	if ((options & K_EVGROUP_ALL) != 0U) {
		control_bits |= EVENT_FLAGS_WAIT_FOR_ALL;
	}

	if (options & K_EVGROUP_CLEAR) {
		control_bits |= EVENT_FLAGS_CLEAR_BITS;
	}

	return control_bits;
}

void z_impl_k_evgroup_init(struct k_evgroup *evgroup)
{
	sys_trace_evflag_init(evgroup);
	z_waitq_init(&evgroup->wait_q);
	evgroup->flags = 0U;
	z_object_init(evgroup);
	sys_trace_end_call(SYS_TRACE_ID_EVFLAG_INIT);
}

void z_impl_k_evgroup_set(struct k_evgroup *evgroup, uint32_t flags)
{
	struct thread_event *ev;

	k_spinlock_key_t key;
	uint32_t control_bits, waited_for_flags, bits_to_clear = 0U;
	bool match = false;
	struct k_thread *thread;

	sys_trace_evflag_set(evgroup, flags);
	/* Set the new bits */
	evgroup->flags |= flags;
	LOG_INF("eventflag %p set to 0x%x", evgroup, evgroup->flags);
	key = k_spin_lock(&lock);

	/* Get the node (thread) in head if there is one and check if new bit
	 * unblocks it.
	 * FIXME: this is a workaround for now, we need some better internal
	 * APIs to iterate on wait queues that work for both dumb and scalable
	 * qait queues. This currently only works with dumb queue.
	 */
	sys_dnode_t *node = z_waitq_head_node(&evgroup->wait_q);

	/* Check if the new bit value should unblock any threads. */
	while(node != NULL) {
		thread = (struct k_thread *)node;
		/*
		 * Get the next node in the wait queue before we do anything to
		 * the queue
		 */
		node = z_waitq_next_node(&evgroup->wait_q, node);

		LOG_INF("Pending thread found: %p", thread);
		ev = (struct thread_event *)sys_dlist_peek_head_not_empty(
			&thread->event_groups);
		waited_for_flags = ev->flags;
		control_bits = ev->flags & EVENT_FLAGS_CONTROL_BITS;
		waited_for_flags &= ~EVENT_FLAGS_CONTROL_BITS;

		LOG_INF("control bits 0x%x, waited for flags: 0x%x",
			control_bits, waited_for_flags);
		if ((control_bits & EVENT_FLAGS_WAIT_FOR_ALL) != 0U) {
			if ((evgroup->flags & waited_for_flags) ==
			    waited_for_flags) {
				match = true;
			}
		} else {
			if ((evgroup->flags & waited_for_flags) != 0U) {
				match = true;
			}
		}

		if (match == true) {
			LOG_INF("Found match in a pending thread, making it ready");
			/* found a match, check if we need to clear */
			if ((control_bits & EVENT_FLAGS_CLEAR_BITS) != 0U) {
				LOG_INF("Clearing bits");
				bits_to_clear |= waited_for_flags;
			}

			sys_dlist_get(&thread->event_groups);

			z_unpend_thread_no_timeout(thread);
			arch_thread_return_value_set(thread, 0);
			z_ready_thread(thread);
		}

	}

	if (bits_to_clear) {
		evgroup->flags &= ~bits_to_clear;
		LOG_INF("Cleared flags: 0x%x", evgroup->flags);
	}

	z_reschedule(&lock, key);

	sys_trace_end_call(SYS_TRACE_ID_EVFLAG_SET);
}

uint32_t z_impl_k_evgroup_get(struct k_evgroup *evgroup)
{
	return evgroup->flags;
}

void z_impl_k_evgroup_clear(struct k_evgroup *evgroup, uint32_t flags)
{
	if (flags == 0U) {
		flags = 0x7fffffff;
	}
	evgroup->flags &= ~(flags);
}

uint32_t z_impl_k_evgroup_wait(struct k_evgroup *evgroup, uint32_t flags,
				 uint8_t options, k_timeout_t timeout)
{
	int ret = 0;
	uint32_t event_bits, control_bits = 0;
	struct thread_event thread_ev;

	sys_trace_evflag_wait(evgroup, flags);
	k_spinlock_key_t key = k_spin_lock(&lock);

	if (check_event_flags(evgroup->flags, flags, options) != false) {
		if (options & K_EVGROUP_CLEAR) {
			evgroup->flags &= ~(flags);
			LOG_INF("eventflag cleared: 0x%x", evgroup->flags);
		}
		ret = evgroup->flags;
		k_spin_unlock(&lock, key);
	} else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		ret = evgroup->flags;
		k_spin_unlock(&lock, key);
	} else {
		control_bits = set_control_bits(options);
		thread_ev.flags = (flags | control_bits);
		sys_dlist_append(&_current->event_groups, &thread_ev._node);
		LOG_INF("pending %p on event flags: 0x%x", _current, flags);
		ret = z_pend_curr(&lock, key, &evgroup->wait_q, timeout);
	}
	sys_trace_end_call(SYS_TRACE_ID_EVFLAG_WAIT);

	return ret;
}
