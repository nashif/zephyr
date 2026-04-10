/*
 * Copyright (c) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_KERNEL_THREAD_NOTIFY_H_
#define ZEPHYR_INCLUDE_KERNEL_THREAD_NOTIFY_H_

/**
 * @file
 * @brief Direct-to-thread notifications
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup thread_notify_apis Direct-to-Thread Notification APIs
 * @ingroup kernel_apis
 * @{
 *
 * Direct-to-thread notifications allow any thread or ISR to signal a
 * specific thread without requiring a separate kernel object.  Each thread
 * carries a 32-bit @em notification @em value and a @em pending @em count.
 *
 * Sending side actions (passed as @p action to k_thread_notify()):
 *
 * - @ref K_THREAD_NOTIFY_SET_BITS    — bitwise-OR @p value into the target's
 *   notification value and increment the pending count.
 * - @ref K_THREAD_NOTIFY_SET_VALUE   — overwrite the target's notification
 *   value with @p value and increment the pending count.  Fails with
 *   -EBUSY if the target already has a pending notification (@em without
 *   @em overwrite variant).
 * - @ref K_THREAD_NOTIFY_INCREMENT   — ignore @p value and simply increment
 *   the pending count (lightweight semaphore give).
 * - @ref K_THREAD_NOTIFY_NO_ACTION   — wake the target thread without
 *   modifying the notification value or pending count.
 *
 * Receiving side:
 * - k_thread_notify_wait()  — wait for any notification, with optional
 *   bit-masking on entry and exit.
 * - k_thread_notify_take()  — wait until the pending count is > 0, then
 *   decrement it (lightweight semaphore take).
 *
 * @note These APIs are only available when CONFIG_THREAD_NOTIFY=y.
 */

/** @brief Notify action: bitwise-OR @p value into notification value */
#define K_THREAD_NOTIFY_SET_BITS    0

/**
 * @brief Notify action: overwrite notification value; fail if already pending.
 *
 * If the target thread already has a pending notification (pending count > 0),
 * k_thread_notify() returns -EBUSY and does not overwrite the value.
 */
#define K_THREAD_NOTIFY_SET_VALUE   1

/**
 * @brief Notify action: overwrite notification value unconditionally.
 */
#define K_THREAD_NOTIFY_SET_VALUE_OVERWRITE 2

/** @brief Notify action: increment pending count only (lightweight sem give) */
#define K_THREAD_NOTIFY_INCREMENT   3

/** @brief Notify action: wake target thread without modifying value/count */
#define K_THREAD_NOTIFY_NO_ACTION   4

/**
 * @brief Send a direct notification to a thread.
 *
 * Posts a notification to @p thread.  The @p action parameter controls how
 * @p value is applied to the thread's notification state.
 *
 * This function may be called from any context, including ISRs.
 *
 * @param thread  Target thread identifier.
 * @param value   Notification value (ignored when action is
 *                K_THREAD_NOTIFY_INCREMENT or K_THREAD_NOTIFY_NO_ACTION).
 * @param action  One of K_THREAD_NOTIFY_*.
 *
 * @retval 0       on success.
 * @retval -EINVAL if @p thread is NULL.
 * @retval -EBUSY  if action is K_THREAD_NOTIFY_SET_VALUE and the target
 *                 already has a pending notification.
 */
int k_thread_notify(k_tid_t thread, uint32_t value, int action);

/**
 * @brief Wait for a direct notification.
 *
 * Suspends the calling thread until a notification arrives or @p timeout
 * expires.  Returns the notification value at the time of wakeup.
 *
 * @param bits_clear_on_entry  Bits to clear in the notification value
 *                             @em before waiting (0 to skip).
 * @param bits_clear_on_exit   Bits to clear in the notification value
 *                             @em after a notification is received (0 to keep
 *                             all bits; pass UINT32_MAX to clear everything).
 * @param value_out            Optional pointer to store the received
 *                             notification value (may be NULL).
 * @param timeout              Maximum time to wait.
 *
 * @retval 0       if a notification was received.
 * @retval -EAGAIN if the operation timed out.
 */
int k_thread_notify_wait(uint32_t bits_clear_on_entry,
			 uint32_t bits_clear_on_exit,
			 uint32_t *value_out,
			 k_timeout_t timeout);

/**
 * @brief Increment a thread's notification pending count.
 *
 * Convenience wrapper for k_thread_notify() with action
 * K_THREAD_NOTIFY_INCREMENT.  Acts like a lightweight semaphore give.
 *
 * @param thread  Target thread identifier.
 *
 * @retval 0 on success.
 */
static inline int k_thread_notify_give(k_tid_t thread)
{
	return k_thread_notify(thread, 0U, K_THREAD_NOTIFY_INCREMENT);
}

/**
 * @brief Wait until the notification pending count becomes non-zero.
 *
 * Blocks until the calling thread's pending count is greater than zero,
 * then decrements it.  Acts like a lightweight semaphore take.
 *
 * @param clear_on_exit  If true, the notification value is also cleared to 0
 *                       on successful return.
 * @param timeout        Maximum time to wait.
 *
 * @retval 0       if a pending notification was consumed.
 * @retval -EAGAIN if the operation timed out.
 */
int k_thread_notify_take(bool clear_on_exit, k_timeout_t timeout);

/**
 * @brief Query the current notification value of a thread.
 *
 * Returns a snapshot of @p thread's notification value.  This is an
 * advisory read-only query; no synchronisation guarantees are provided.
 *
 * @param thread  Target thread identifier.
 *
 * @return current notification value.
 */
uint32_t k_thread_notify_value_get(k_tid_t thread);

/**
 * @brief Clear the notification state of the calling thread.
 *
 * Resets both the notification value and the pending count to zero.
 * Intended for initialisation or error-recovery situations.
 */
void k_thread_notify_clear(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_KERNEL_THREAD_NOTIFY_H_ */
