/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <misc/printk.h>
#include <misc/util.h>
#include <zephyr.h>

#include <rtt/SEGGER_RTT.h>
#include <systemview/SEGGER_SYSVIEW.h>

K_THREAD_STACK_DEFINE(printer_stack, 1024);
K_THREAD_STACK_DEFINE(calc_stack, 1024);

static struct k_thread printer_thread_data;
static struct k_thread calc_thread_data;

K_SEM_DEFINE(sema, 0, 1);

static void printer_thread(void)
{
	k_sem_take(&sema, K_FOREVER);
	for (;;) {
		SEGGER_SYSVIEW_Print("Printer thread says hello");
		k_sleep(MSEC_PER_SEC);
	}
}

static void calc_thread(void)
{
	int denom = 0;

	for (;;) {
		const int val = 0xbebacafe;

		denom = (denom + 1) % 16;

		if (denom == 0) {
			SEGGER_SYSVIEW_Warn("Not calculating: denom is 0");
		} else {
			SEGGER_SYSVIEW_PrintfHost("Calculated: %d",
						  val / denom);
		}

		k_sleep(MSEC_PER_SEC);
		k_sem_give(&sema);
	}
}

void main(void)
{

	k_thread_create(&printer_thread_data, printer_stack,
			K_THREAD_STACK_SIZEOF(printer_stack),
			(k_thread_entry_t)printer_thread,
			NULL, NULL, NULL, K_PRIO_PREEMPT(3), 0, 0);

	k_thread_create(&calc_thread_data, calc_stack,
			K_THREAD_STACK_SIZEOF(calc_stack),
			(k_thread_entry_t)calc_thread,
			NULL, NULL, NULL, K_PRIO_PREEMPT(5), 0, 0);
}
