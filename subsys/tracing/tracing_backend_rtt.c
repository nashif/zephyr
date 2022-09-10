/*
 * Copyright (c) 2019 Intel corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Disable syscall tracing for all calls from this compilation unit to avoid
 * undefined symbols as the macros are not expanded recursively
 */
#define DISABLE_SYSCALL_TRACING

#include <errno.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/__assert.h>
#include <tracing_core.h>
#include <tracing_buffer.h>
#include <tracing_backend.h>
#include <SEGGER_RTT.h>


#define RTT_BUFFER_SIZE 0
static uint8_t rtt_buf[RTT_BUFFER_SIZE];

#define RTT_RETRY_DELAY_MS 10
#define RTT_BUFFER 0


#define RTT_LOCK() \
	COND_CODE_0(RTT_BUFFER, (SEGGER_RTT_LOCK()), ())

#define RTT_UNLOCK() \
	COND_CODE_0(RTT_BUFFER, (SEGGER_RTT_UNLOCK()), ())

static bool host_present;

static void on_failed_write(int retry_cnt)
{
	if (retry_cnt == 0) {
		host_present = false;
	} else {
		k_busy_wait(USEC_PER_MSEC * RTT_RETRY_DELAY_MS);
	}
}

static void on_write(int retry_cnt)
{
	host_present = true;
}


static void tracing_backend_rtt_output(
	const struct tracing_backend *backend,
	uint8_t *data, uint32_t length)
{
	int ret = 0;
	int retry_cnt = 4;

	do {

		RTT_LOCK();
		ret = SEGGER_RTT_WriteSkipNoLock(RTT_BUFFER, data, length);
		RTT_UNLOCK();

		if (ret) {
			on_write(retry_cnt);
		} else if (host_present) {
			retry_cnt--;
			on_failed_write(retry_cnt);
		} else {
		}
	} while ((ret == 0) && host_present);
}



static void tracing_backend_rtt_init(void)
{
	SEGGER_RTT_ConfigUpBuffer(RTT_BUFFER, "Tracing",
				  rtt_buf, sizeof(rtt_buf),
				  SEGGER_RTT_MODE_NO_BLOCK_SKIP);

}

const struct tracing_backend_api tracing_backend_rtt_api = {
	.init = tracing_backend_rtt_init,
	.output = tracing_backend_rtt_output
};

TRACING_BACKEND_DEFINE(tracing_backend_rtt, tracing_backend_rtt_api);
