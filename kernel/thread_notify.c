/*
 * Copyright (c) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Direct-to-thread notification implementation.
 *
 * Provides a lightweight signalling mechanism that allows any thread or
 * ISR to post a 32-bit value directly to a target thread, avoiding the
 * overhead of creating a separate kernel object (semaphore, event, etc.)
 * for simple notification patterns.
 */

#include <zephyr/kernel.h>
#include <zephyr/kernel/thread_notify.h>
#include <ksched.h>
#include <wait_q.h>

/* A single system-wide spinlock protects all per-thread notify state.
 * Per-thread locking would reduce contention on heavily multi-core
 * systems but adds per-thread lock overhead.  Revisit if profiling shows
 * this lock is a contention point.
 */
static struct k_spinlock notify_lock;

/**
 * @brief Initialise notification state for a newly created thread.
 *
 * Called from thread.c during thread creation with interrupts locked.
 *
 * @param thread  Thread whose notification fields should be initialised.
 */
void z_thread_notify_init(struct k_thread *thread)
{
	thread->notify_value   = 0U;
	thread->notify_pending = 0U;
	z_waitq_init(&thread->notify_wait_q);
}

/* -------------------------------------------------------------------------
 * Sending side
 * -------------------------------------------------------------------------
 */

int k_thread_notify(k_tid_t thread, uint32_t value, int action)
{
	if (thread == NULL) {
		return -EINVAL;
	}

	k_spinlock_key_t key = k_spin_lock(&notify_lock);
	struct k_thread *waiter;
	bool resched = false;

	switch (action) {
	case K_THREAD_NOTIFY_SET_BITS:
		thread->notify_value |= value;
		thread->notify_pending++;
		break;

	case K_THREAD_NOTIFY_SET_VALUE:
		/* Non-overwriting set: fail if a notification is already pending */
		if (thread->notify_pending > 0U) {
			k_spin_unlock(&notify_lock, key);
			return -EBUSY;
		}
		thread->notify_value = value;
		thread->notify_pending++;
		break;

	case K_THREAD_NOTIFY_SET_VALUE_OVERWRITE:
		thread->notify_value = value;
		thread->notify_pending++;
		break;

	case K_THREAD_NOTIFY_INCREMENT:
		thread->notify_pending++;
		break;

	case K_THREAD_NOTIFY_NO_ACTION:
		/* Just wake any waiting thread; do not touch state */
		break;

	default:
		k_spin_unlock(&notify_lock, key);
		return -EINVAL;
	}

	/* Wake the first thread waiting in k_thread_notify_wait() or
	 * k_thread_notify_take() for this target thread, if any.
	 */
	waiter = z_unpend_first_thread(&thread->notify_wait_q);
	if (waiter != NULL) {
		arch_thread_return_value_set(waiter, 0);
		z_ready_thread(waiter);
		resched = true;
	}

	if (resched) {
		z_reschedule(&notify_lock, key);
	} else {
		k_spin_unlock(&notify_lock, key);
	}

	return 0;
}

/* -------------------------------------------------------------------------
 * Receiving side
 * -------------------------------------------------------------------------
 */

int k_thread_notify_wait(uint32_t bits_clear_on_entry,
			 uint32_t bits_clear_on_exit,
			 uint32_t *value_out,
			 k_timeout_t timeout)
{
	__ASSERT_NO_MSG(!arch_is_in_isr());

	k_spinlock_key_t key = k_spin_lock(&notify_lock);

	/* Clear requested entry bits before checking */
	_current->notify_value &= ~bits_clear_on_entry;

	if (_current->notify_pending > 0U) {
		/* Notification already queued; consume it without blocking */
		_current->notify_pending--;
		if (value_out != NULL) {
			*value_out = _current->notify_value;
		}
		_current->notify_value &= ~bits_clear_on_exit;
		k_spin_unlock(&notify_lock, key);
		return 0;
	}

	if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		k_spin_unlock(&notify_lock, key);
		return -EAGAIN;
	}

	int ret = z_pend_curr(&notify_lock, key,
			      &_current->notify_wait_q, timeout);

	if (ret == 0) {
		/* Re-acquire to read and clear notification state */
		key = k_spin_lock(&notify_lock);

		if (_current->notify_pending > 0U) {
			_current->notify_pending--;
		}
		if (value_out != NULL) {
			*value_out = _current->notify_value;
		}
		_current->notify_value &= ~bits_clear_on_exit;

		k_spin_unlock(&notify_lock, key);
	}

	return ret;
}

int k_thread_notify_take(bool clear_on_exit, k_timeout_t timeout)
{
	__ASSERT_NO_MSG(!arch_is_in_isr());

	k_spinlock_key_t key = k_spin_lock(&notify_lock);

	if (_current->notify_pending > 0U) {
		_current->notify_pending--;
		if (clear_on_exit) {
			_current->notify_value = 0U;
		}
		k_spin_unlock(&notify_lock, key);
		return 0;
	}

	if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		k_spin_unlock(&notify_lock, key);
		return -EAGAIN;
	}

	int ret = z_pend_curr(&notify_lock, key,
			      &_current->notify_wait_q, timeout);

	if (ret == 0) {
		key = k_spin_lock(&notify_lock);

		if (_current->notify_pending > 0U) {
			_current->notify_pending--;
		}
		if (clear_on_exit) {
			_current->notify_value = 0U;
		}

		k_spin_unlock(&notify_lock, key);
	}

	return ret;
}

/* -------------------------------------------------------------------------
 * Utility helpers
 * -------------------------------------------------------------------------
 */

uint32_t k_thread_notify_value_get(k_tid_t thread)
{
	k_spinlock_key_t key = k_spin_lock(&notify_lock);
	uint32_t val = (thread != NULL) ? thread->notify_value : 0U;

	k_spin_unlock(&notify_lock, key);
	return val;
}

void k_thread_notify_clear(void)
{
	k_spinlock_key_t key = k_spin_lock(&notify_lock);

	_current->notify_value   = 0U;
	_current->notify_pending = 0U;

	k_spin_unlock(&notify_lock, key);
}
