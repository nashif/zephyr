/*
 * Copyright (c) 2024 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Same two patterns as samples/kernel/completion, implemented
 *        with semaphores instead of completions.
 *
 * Comparing the two side-by-side surfaces the behavioural differences:
 *
 * Pattern 1 – one-shot synchronisation
 * ------------------------------------
 * Identical in both primitives: initialise with count 0, give/complete
 * after the work is done, take/wait before consuming the result.
 *
 * Pattern 2 – broadcast fan-out
 * ------------------------------
 * With k_completion, k_completion_complete_all() releases all N waiting
 * threads in one call and permanently opens the gate (done = UINT_MAX).
 *
 * With k_sem there is no broadcast operation.  To release N threads the
 * sender must call k_sem_give() exactly N times.  The semaphore count is
 * consumed by each taker, so it returns to zero after all N threads have
 * run – there is no permanent "open gate" unless the limit is set high
 * enough and the count is never decremented.
 *
 * This distinction matters when:
 *   - The number of waiters is not known at signal time.
 *   - New threads must pass through the gate *after* the initial wave of
 *     waiters has already been woken (completion: automatic pass-through;
 *     semaphore: already consumed, next taker blocks again).
 *   - One needs to re-arm the gate for the next round (both offer reset,
 *     but for completions reset also aborts pending waiters with -EAGAIN).
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* ------------------------------------------------------------------ */
/* Pattern 1: one-shot worker                                          */
/* ------------------------------------------------------------------ */

#define WORKER_STACK_SIZE 512
#define WORKER_PRIORITY   5

static K_THREAD_STACK_DEFINE(worker_stack, WORKER_STACK_SIZE);
static struct k_thread worker_thread;

/* The compute result shared between the worker and main. */
static volatile int compute_result;

/*
 * Semaphore used to signal that the result is ready.
 * Initial count 0 (not ready), limit 1 (binary semaphore).
 * Equivalent to: k_completion_init(&result_ready);
 */
static K_SEM_DEFINE(result_ready, 0, 1);

static void worker_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	printk("worker: starting long computation\n");

	/* Simulate time-consuming work. */
	k_sleep(K_MSEC(200));
	compute_result = 42;

	printk("worker: computation done, giving semaphore\n");
	/*
	 * k_sem_give()  ≈  k_completion_complete()
	 * If no thread is waiting the count is incremented so that the
	 * next k_sem_take() returns immediately – same as completion.
	 */
	k_sem_give(&result_ready);
}

/* ------------------------------------------------------------------ */
/* Pattern 2: broadcast / fan-out                                      */
/* ------------------------------------------------------------------ */

#define N_TASKS 3

static K_THREAD_STACK_ARRAY_DEFINE(task_stacks, N_TASKS, WORKER_STACK_SIZE);
static struct k_thread task_threads[N_TASKS];

/*
 * Semaphore used to start all tasks simultaneously.
 *
 * KEY DIFFERENCE vs. completion:
 *   k_completion_complete_all() wakes all N waiters with a single call.
 *   With k_sem there is no equivalent – the sender must call k_sem_give()
 *   N times, once per waiting thread.  The limit is set to N so the count
 *   can reach N before any thread runs.
 */
static K_SEM_DEFINE(start_signal, 0, N_TASKS);

/*
 * Each finished task gives this semaphore once.
 * main() takes it N_TASKS times – same counting pattern as the completion
 * version, and here the two primitives behave identically.
 */
static K_SEM_DEFINE(tasks_done, 0, N_TASKS);

static void task_entry(void *p1, void *p2, void *p3)
{
	int id = (int)(intptr_t)p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Wait for the start signal – same as k_completion_wait(). */
	k_sem_take(&start_signal, K_FOREVER);

	/* Simulate variable-length work. */
	k_sleep(K_MSEC(50 * (id + 1)));
	printk("worker: task %d done\n", id + 1);

	/* Signal completion of this task. */
	k_sem_give(&tasks_done);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
	/* --- Pattern 1 --- */
	/* (semaphores initialised statically above via K_SEM_DEFINE) */

	k_thread_create(&worker_thread, worker_stack, WORKER_STACK_SIZE,
			worker_entry, NULL, NULL, NULL,
			WORKER_PRIORITY, 0, K_NO_WAIT);

	/*
	 * Block until the worker finishes.
	 * k_sem_take() ≈ k_completion_wait()
	 */
	k_sem_take(&result_ready, K_FOREVER);
	printk("main: computation result ready: %d\n", compute_result);

	k_thread_join(&worker_thread, K_FOREVER);

	/* --- Pattern 2 --- */

	/* Spawn all task threads; they immediately pend on start_signal. */
	for (int i = 0; i < N_TASKS; i++) {
		k_thread_create(&task_threads[i], task_stacks[i],
				WORKER_STACK_SIZE,
				task_entry, (void *)(intptr_t)i, NULL, NULL,
				WORKER_PRIORITY, 0, K_NO_WAIT);
	}

	/*
	 * Release all tasks.
	 *
	 * With completion:   k_completion_complete_all(&start_signal);
	 *
	 * With semaphore: no broadcast – give N times, once per waiter.
	 * If N is not known at this point the caller would need to track
	 * it separately; k_completion_complete_all() avoids that entirely.
	 */
	for (int i = 0; i < N_TASKS; i++) {
		k_sem_give(&start_signal);
	}

	/* Wait for every task to finish (N_TASKS separate takes). */
	for (int i = 0; i < N_TASKS; i++) {
		k_sem_take(&tasks_done, K_FOREVER);
	}

	printk("main: all tasks done\n");

	for (int i = 0; i < N_TASKS; i++) {
		k_thread_join(&task_threads[i], K_FOREVER);
	}

	return 0;
}
