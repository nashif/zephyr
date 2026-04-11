/*
 * Copyright (c) 2024 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Sample demonstrating k_completion.
 *
 * This sample shows two common usage patterns for completion objects:
 *
 * Pattern 1 – one-shot synchronisation
 * ------------------------------------
 * A worker thread performs a time-consuming computation and signals a
 * completion when it finishes.  The main thread waits on the completion
 * before consuming the result.
 *
 * Pattern 2 – broadcast to multiple waiters
 * ------------------------------------------
 * Three worker threads each wait on an individual completion.  A
 * coordinator completes them all at once via k_completion_complete_all()
 * and waits for all workers to finish via a shared "done" completion.
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

/* Completion used to signal that the result is ready. */
static struct k_completion result_ready;

static void worker_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	printk("worker: starting long computation\n");

	/* Simulate time-consuming work. */
	k_sleep(K_MSEC(200));
	compute_result = 42;

	printk("worker: computation done, signalling completion\n");
	k_completion_complete(&result_ready);
}

/* ------------------------------------------------------------------ */
/* Pattern 2: broadcast / fan-out                                      */
/* ------------------------------------------------------------------ */

#define N_TASKS 3

static K_THREAD_STACK_ARRAY_DEFINE(task_stacks, N_TASKS, WORKER_STACK_SIZE);
static struct k_thread task_threads[N_TASKS];

/* Coordinator signals this to start all tasks simultaneously. */
static struct k_completion start_signal;

/* Each finished task calls complete() on this shared completion.    */
/* The main thread waits N_TASKS times (one per task).               */
static struct k_completion tasks_done;

static void task_entry(void *p1, void *p2, void *p3)
{
	int id = (int)(intptr_t)p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Wait for the coordinator's start signal. */
	k_completion_wait(&start_signal, K_FOREVER);

	/* Simulate variable-length work. */
	k_sleep(K_MSEC(50 * (id + 1)));
	printk("worker: task %d done\n", id + 1);

	/* Signal the "done" completion once per finished task. */
	k_completion_complete(&tasks_done);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
	/* --- Pattern 1 --- */
	k_completion_init(&result_ready);

	k_thread_create(&worker_thread, worker_stack, WORKER_STACK_SIZE,
			worker_entry, NULL, NULL, NULL,
			WORKER_PRIORITY, 0, K_NO_WAIT);

	/* Block until the worker finishes. */
	k_completion_wait(&result_ready, K_FOREVER);
	printk("main: computation result ready: %d\n", compute_result);

	k_thread_join(&worker_thread, K_FOREVER);

	/* --- Pattern 2 --- */
	k_completion_init(&start_signal);
	k_completion_init(&tasks_done);

	/* Spawn all task threads; they pend on start_signal immediately. */
	for (int i = 0; i < N_TASKS; i++) {
		k_thread_create(&task_threads[i], task_stacks[i],
				WORKER_STACK_SIZE,
				task_entry, (void *)(intptr_t)i, NULL, NULL,
				WORKER_PRIORITY, 0, K_NO_WAIT);
	}

	/* Release all tasks at once. */
	k_completion_complete_all(&start_signal);

	/* Wait for every task to finish (N_TASKS separate signals). */
	for (int i = 0; i < N_TASKS; i++) {
		k_completion_wait(&tasks_done, K_FOREVER);
	}

	printk("main: all tasks done\n");

	for (int i = 0; i < N_TASKS; i++) {
		k_thread_join(&task_threads[i], K_FOREVER);
	}

	return 0;
}
