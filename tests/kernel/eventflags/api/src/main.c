/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(evgroup_test, LOG_LEVEL_DBG);

struct k_evgroup evgroup1;

/* size of stack area used by each thread */
#define STACK_SIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 6

/* delay between greetings (in ms) */
#define SLEEPTIME 500
K_EVGROUP_DEFINE(evgroup);

static K_THREAD_STACK_DEFINE(thread_stack1, STACK_SIZE);
static K_THREAD_STACK_DEFINE(thread_stack2, STACK_SIZE);
struct k_thread thread_data1, thread_data2;

#define FIRST_BIT BIT(0)
#define SECOND_BIT BIT(1)

int flag_match = 0;

void thread_wait(void *t, void *dummy2, void *dummy3)
{
	int res;

	int tid = POINTER_TO_INT(t);

	while (1) {
		printk("waiting thread %s\n",
		       k_thread_name_get(k_current_get()));

		res = k_evgroup_wait(&evgroup, 0x3,
				       K_EVGROUP_ALL | K_EVGROUP_CLEAR,
				       K_FOREVER);
		if (res >= 0) {
			printk("Both flags raised: 0x%x\n", evgroup.flags);
			flag_match = 1;
		} else if (res == -EAGAIN) {
			printk("Timeout\n");
		}
	}
}

void main(void)
{
#if 1
	k_thread_create(&thread_data1, thread_stack1, STACK_SIZE, thread_wait,
			INT_TO_POINTER(1), NULL, NULL, K_PRIO_PREEMPT(PRIORITY),
			0, K_FOREVER);

	k_thread_name_set(&thread_data1, "thread1");
	k_thread_start(&thread_data1);

#endif
#if 1
	k_thread_create(&thread_data2, thread_stack2, STACK_SIZE, thread_wait,
			INT_TO_POINTER(2), NULL, NULL, K_PRIO_PREEMPT(PRIORITY),
			0, K_FOREVER);

	k_thread_name_set(&thread_data2, "thread2");
	k_thread_start(&thread_data2);
#endif
	k_sleep(K_MSEC(500));

	while (1) {
		printk("first action\n");
		k_evgroup_set(&evgroup, FIRST_BIT);
		printk("second action\n");
		k_evgroup_set(&evgroup, SECOND_BIT);
		k_sleep(K_MSEC(500));
	}
}
