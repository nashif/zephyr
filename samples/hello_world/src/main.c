/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(hello_world, LOG_LEVEL_DBG);

struct k_eventflag evflag;


/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

/* delay between greetings (in ms) */
#define SLEEPTIME 500



K_EVENTFLAG_DEFINE(evflag);

#if 0

/* threadB is a dynamic thread that is spawned by threadA */

void threadB(void *dummy1, void *dummy2, void *dummy3)
{
	int ret;

	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);
	printk("in threadB\n");

	while(1) {
		ret = k_eventflag_get(&evflag);
		printk("threadB: I am here now: 0x%x\n", ret);
		ret = k_eventflag_wait(&evflag, 0x111, K_EVENTFLAGS_AND_CLEAR, K_FOREVER);
		printk("threadB: (after) I am here now: 0x%x\n", ret);
		k_sleep(1000);
	}
}

K_THREAD_STACK_DEFINE(threadB_stack_area, STACKSIZE);
static struct k_thread threadB_data;

/* threadA is a static thread that is spawned automatically */

void threadA(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	/* spawn threadB */
	k_tid_t tid = k_thread_create(&threadB_data, threadB_stack_area,
			STACKSIZE, threadB, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);

	k_thread_name_set(tid, "thread_b");

	printk("in threadA\n");

	while(1) {
		printk("thread A seeping\n");
		k_sleep(1000);
		k_eventflag_set(&evflag, 0x011);
	}
}

K_THREAD_DEFINE(thread_a, STACKSIZE, threadA, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

#endif


#define ON_BIT BIT(0)

void led_on(void *dummy1, void *dummy2, void *dummy3)
{
	while(1) {
		LOG_INF("LED on");
		k_eventflag_set(&evflag, ON_BIT);
		k_sleep(15000);

	}
}


void led_off(void *dummy1, void *dummy2, void *dummy3)
{
	int ret;
	while(1) {
		ret = k_eventflag_wait(&evflag, ON_BIT, K_EVENTFLAGS_OR_CLEAR, K_FOREVER);
		LOG_INF("LED off (0x%x)", ret);
		k_sleep(50);
	}
}


K_THREAD_DEFINE(led_on_thread, STACKSIZE, led_on, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

K_THREAD_DEFINE(led_off_thread, STACKSIZE, led_off, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

#if 0

void main(void)
{
	u32_t flag;

	k_eventflag_init(&evflag);
	k_eventflag_set(&evflag, 0x100001U);
	k_eventflag_set(&evflag, 0x100010U);

	flag = k_eventflag_get(&evflag);

	printk("flags 0x%x\n", flag);

	k_eventflag_clear(&evflag, 0);
	flag = k_eventflag_get(&evflag);

	printk("flags after clearing 0x%x\n", flag);

}
#endif
