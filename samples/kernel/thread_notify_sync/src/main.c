/*
 * Copyright (c) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Same two synchronisation patterns as samples/kernel/completion
 *        and samples/kernel/semaphore_sync, implemented with direct-to-thread
 *        notifications (CONFIG_THREAD_NOTIFY).
 *
 * Thread notifications live inside the target thread itself – no separate
 * kernel object is needed.  This changes the programming model noticeably:
 *
 * Pattern 1 – one-shot synchronisation
 * ------------------------------------
 * Functionally equivalent to both the completion and semaphore versions.
 * The worker calls k_thread_notify_give(main_tid) instead of signalling
 * a shared object; the main thread calls k_thread_notify_take() instead
 * of waiting on that object.
 *
 * API mapping
 * ~~~~~~~~~~~
 *   Completion                       Semaphore               Thread notify
 *   -------------------------------- ----------------------- ----------------------
 *   k_completion_init(&c)            k_sem_init(&s,0,1)      k_thread_notify_clear()
 *   k_completion_complete(&c)        k_sem_give(&s)          k_thread_notify_give(tid)
 *   k_completion_wait(&c, tmo)       k_sem_take(&s, tmo)     k_thread_notify_take(…)
 *
 * Pattern 2 – broadcast fan-out
 * ------------------------------
 * This is where all three primitives diverge:
 *
 *   k_completion_complete_all(&c)
 *     One call, wakes all N waiters, gate stays open permanently.
 *
 *   for (int i = 0; i < N; i++) k_sem_give(&start_signal);
 *     N calls needed; counts are consumed, so a late arrival blocks again.
 *     Number of waiters must be known at signal time.
 *
 *   for (int i = 0; i < N; i++) k_thread_notify_give(task_tid[i]);
 *     N calls needed (same as semaphore).
 *     Requires the TID of every target thread, not a shared object.
 *     No "open gate" semantics – a new thread cannot be added without
 *     explicitly registering its TID and sending another notification.
 *     Each thread carries its own notification state independently.
 *
 * Key unique properties of thread notify
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * - No separate kernel object: the notification state is part of the
 *   thread control block (struct k_thread).
 * - The sender addresses a specific thread by TID; there is no
 *   anonymous "post to whoever is waiting" semantic.
 * - k_thread_notify() can also carry a 32-bit value and use actions
 *   (SET_BITS, SET_VALUE, INCREMENT, NO_ACTION), which completions and
 *   semaphores do not offer.
 * - Only the target thread can wait on its own notification; multiple
 *   threads cannot pend on the same notification object.
 */

#include <zephyr/kernel.h>
#include <zephyr/kernel/thread_notify.h>
#include <zephyr/sys/printk.h>

/* ------------------------------------------------------------------ */
/* Pattern 1: one-shot worker                                          */
/* ------------------------------------------------------------------ */

#define WORKER_STACK_SIZE 512
#define WORKER_PRIORITY   5

static K_THREAD_STACK_DEFINE(worker_stack, WORKER_STACK_SIZE);
static struct k_thread worker_thread;

/* TID of the main thread, set before the worker is spawned. */
static k_tid_t main_tid;

/* The compute result shared between the worker and main. */
static volatile int compute_result;

static void worker_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	printk("worker: starting long computation\n");

	/* Simulate time-consuming work. */
	k_sleep(K_MSEC(200));
	compute_result = 42;

	printk("worker: computation done, notifying main\n");
	/*
	 * k_thread_notify_give(main_tid)  ≈  k_completion_complete(&c)
	 *                                  ≈  k_sem_give(&s)
	 *
	 * Unlike the other two, the destination is a TID, not an object.
	 */
	k_thread_notify_give(main_tid);
}

/* ------------------------------------------------------------------ */
/* Pattern 2: broadcast / fan-out                                      */
/* ------------------------------------------------------------------ */

#define N_TASKS 3

static K_THREAD_STACK_ARRAY_DEFINE(task_stacks, N_TASKS, WORKER_STACK_SIZE);
static struct k_thread task_threads[N_TASKS];

/* TID array filled by k_thread_create(); needed to address each task. */
static k_tid_t task_tids[N_TASKS];

/*
 * Each task thread needs to know its own TID to wait on its notification,
 * and the main TID to signal "done".  Pass both through p1/p2.
 *
 * With a completion or semaphore, threads would simply pend on a shared
 * object – no per-thread bookkeeping is needed on the sender side.
 */
static void task_entry(void *p1, void *p2, void *p3)
{
	int id = (int)(intptr_t)p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/*
	 * Wait for the coordinator's start notification.
	 *
	 *   completion: k_completion_wait(&start_signal, K_FOREVER)
	 *   semaphore:  k_sem_take(&start_signal, K_FOREVER)
	 *   notify:     k_thread_notify_take(false, K_FOREVER)
	 *
	 * The thread waits on *its own* notification state – no shared
	 * object is involved.
	 */
	k_thread_notify_take(false, K_FOREVER);

	/* Simulate variable-length work. */
	k_sleep(K_MSEC(50 * (id + 1)));
	printk("worker: task %d done\n", id + 1);

	/*
	 * Signal the main thread directly.
	 *
	 *   completion: k_completion_complete(&tasks_done)
	 *   semaphore:  k_sem_give(&tasks_done)
	 *   notify:     k_thread_notify_give(main_tid)
	 */
	k_thread_notify_give(main_tid);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
	/* --- Pattern 1 --- */
	main_tid = k_current_get();

	/*
	 * Clear any leftover notification state – analogous to initialising
	 * the completion or semaphore to "not done / count = 0".
	 */
	k_thread_notify_clear();

	k_thread_create(&worker_thread, worker_stack, WORKER_STACK_SIZE,
			worker_entry, NULL, NULL, NULL,
			WORKER_PRIORITY, 0, K_NO_WAIT);

	/*
	 * Block until the worker finishes.
	 *
	 *   completion: k_completion_wait(&result_ready, K_FOREVER)
	 *   semaphore:  k_sem_take(&result_ready, K_FOREVER)
	 *   notify:     k_thread_notify_take(false, K_FOREVER)
	 */
	k_thread_notify_take(false, K_FOREVER);
	printk("main: computation result ready: %d\n", compute_result);

	k_thread_join(&worker_thread, K_FOREVER);

	/* --- Pattern 2 --- */

	/*
	 * Clear the notification pending count before re-using it for
	 * the "all tasks done" counting below.
	 */
	k_thread_notify_clear();

	/* Spawn all task threads; they pend on their own notification.
	 * k_thread_create() returns the TID (= &task_threads[i]).
	 */
	for (int i = 0; i < N_TASKS; i++) {
		task_tids[i] = k_thread_create(&task_threads[i], task_stacks[i],
					       WORKER_STACK_SIZE,
					       task_entry, (void *)(intptr_t)i, NULL, NULL,
					       WORKER_PRIORITY, 0, K_NO_WAIT);
	}

	/*
	 * Release all tasks.
	 *
	 *   completion: k_completion_complete_all(&start_signal)   -- 1 call
	 *   semaphore:  for (i) k_sem_give(&start_signal)          -- N calls
	 *   notify:     for (i) k_thread_notify_give(task_tid[i])  -- N calls
	 *
	 * Both semaphore and thread_notify require N explicit calls.
	 * Additionally, thread_notify requires the TID of each target.
	 */
	for (int i = 0; i < N_TASKS; i++) {
		k_thread_notify_give(task_tids[i]);
	}

	/*
	 * Wait for every task to finish.  Each task gave main_tid one
	 * notification, so take N times.
	 *
	 *   completion: for (i) k_completion_wait(&tasks_done, K_FOREVER)
	 *   semaphore:  for (i) k_sem_take(&tasks_done, K_FOREVER)
	 *   notify:     for (i) k_thread_notify_take(false, K_FOREVER)
	 */
	for (int i = 0; i < N_TASKS; i++) {
		k_thread_notify_take(false, K_FOREVER);
	}

	printk("main: all tasks done\n");

	for (int i = 0; i < N_TASKS; i++) {
		k_thread_join(&task_threads[i], K_FOREVER);
	}

	return 0;
}
