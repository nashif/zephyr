/*
 * Copyright (c) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Direct-to-thread notifications sample
 *
 * Demonstrates two common usage patterns:
 *
 * 1. **Value notification** — a "sensor" thread measures a temperature and
 *    notifies the main thread directly with the reading.  The main thread
 *    waits with k_thread_notify_wait().
 *
 * 2. **Lightweight semaphore** — a "producer" thread enqueues several work
 *    items and signals the consumer via k_thread_notify_give().  The
 *    consumer drains them with k_thread_notify_take().
 */

#include <zephyr/kernel.h>
#include <zephyr/kernel/thread_notify.h>
#include <zephyr/sys/printk.h>

/* --------------------------------------------------------------------------
 * Scenario 1: value notification (sensor -> main)
 * --------------------------------------------------------------------------
 */

#define SENSOR_STACK_SIZE 512
#define SENSOR_PRIORITY   5

static K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
static struct k_thread sensor_thread;

static k_tid_t main_tid;   /* set by main before starting the sensor thread */

static void sensor_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Simulate a sensor reading (temperature = 23 °C = 0x17) */
	k_sleep(K_MSEC(200));

	uint32_t temperature = 23U; /* degrees Celsius */

	printk("[sensor] posting temperature: %u\n", temperature);

	/* Overwrite the notification value with the sensor reading */
	k_thread_notify(main_tid, temperature,
			K_THREAD_NOTIFY_SET_VALUE_OVERWRITE);
}

/* --------------------------------------------------------------------------
 * Scenario 2: lightweight semaphore (producer -> consumer)
 * --------------------------------------------------------------------------
 */

#define PRODUCER_STACK_SIZE 512
#define CONSUMER_STACK_SIZE 512
#define WORK_ITEMS          5

static K_THREAD_STACK_DEFINE(producer_stack, PRODUCER_STACK_SIZE);
static K_THREAD_STACK_DEFINE(consumer_stack, CONSUMER_STACK_SIZE);
static struct k_thread producer_thread;
static struct k_thread consumer_thread;

static k_tid_t consumer_tid;

static void producer_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	printk("[producer] sending %d work items\n", WORK_ITEMS);

	for (int i = 0; i < WORK_ITEMS; i++) {
		k_sleep(K_MSEC(50));
		k_thread_notify_give(consumer_tid);
	}
}

static void consumer_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int processed = 0;

	while (processed < WORK_ITEMS) {
		int ret = k_thread_notify_take(true, K_MSEC(1000));

		if (ret == 0) {
			processed++;
		}
	}

	printk("[consumer] processed %d work items\n", processed);
}

/* --------------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------------
 */

int main(void)
{
	/* ----------------------------------------------------------------
	 * Scenario 1: wait for a value notification from the sensor thread
	 * ----------------------------------------------------------------
	 */
	main_tid = k_current_get();
	k_thread_notify_clear();

	k_thread_create(&sensor_thread, sensor_stack, SENSOR_STACK_SIZE,
			sensor_entry, NULL, NULL, NULL,
			SENSOR_PRIORITY, 0, K_NO_WAIT);

	printk("[main] waiting for sensor notification\n");

	uint32_t val = 0U;
	int ret = k_thread_notify_wait(0U, UINT32_MAX, &val, K_MSEC(1000));

	if (ret == 0) {
		printk("[main] received notification: value=0x%08X\n", val);
	} else {
		printk("[main] timed out waiting for sensor!\n");
	}

	k_thread_join(&sensor_thread, K_FOREVER);

	/* ----------------------------------------------------------------
	 * Scenario 2: producer/consumer with lightweight semaphore
	 * ----------------------------------------------------------------
	 */
	consumer_tid = k_thread_create(&consumer_thread, consumer_stack,
				       CONSUMER_STACK_SIZE,
				       consumer_entry, NULL, NULL, NULL,
				       K_PRIO_PREEMPT(2), 0, K_NO_WAIT);

	k_thread_create(&producer_thread, producer_stack, PRODUCER_STACK_SIZE,
			producer_entry, NULL, NULL, NULL,
			K_PRIO_PREEMPT(3), 0, K_NO_WAIT);

	k_thread_join(&producer_thread, K_FOREVER);
	k_thread_join(&consumer_thread, K_FOREVER);

	printk("Sample complete\n");
	return 0;
}
