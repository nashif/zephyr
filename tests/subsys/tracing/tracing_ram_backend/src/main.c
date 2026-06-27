/*
 * Copyright (c) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <string.h>
#include <tracing_core.h>
#include <tracing_backend.h>

/*
 * The RAM backend exposes its output buffer as a global symbol so that it can
 * be dumped with a debugger. We use it here to verify the backend behaviour.
 */
extern uint8_t ram_tracing[];

#define RAM_SIZE CONFIG_RAM_TRACING_BUFFER_SIZE

static struct tracing_backend *ram_backend;

/*
 * Disable the global tracing pipeline so that the only writes reaching the RAM
 * backend are the ones issued explicitly by the test. Otherwise background
 * kernel instrumentation (thread switches, ISRs, ...) would also be routed to
 * the same backend and make the content of @ref ram_tracing non-deterministic.
 */
static void disable_pipeline(void)
{
	uint8_t cmd[] = "disable";

	tracing_cmd_handle(cmd, sizeof(cmd));
	zassert_false(is_tracing_enabled(), "Failed to disable tracing pipeline");
}

static void ram_output(const void *data, uint32_t length)
{
	ram_backend->api->output(ram_backend, (uint8_t *)data, length);
}

static void *setup(void)
{
	ram_backend = tracing_backend_get("tracing_backend_ram");
	zassert_not_null(ram_backend, "RAM backend not registered");
	zassert_not_null(ram_backend->api, "RAM backend has no api");

	disable_pipeline();

	return NULL;
}

/**
 * @brief Data handed to the RAM backend lands in its buffer.
 */
ZTEST(tracing_ram_backend, test_ram_output_basic)
{
	uint8_t payload[] = "RAM_BACKEND_PAYLOAD";

	ram_backend->api->init();
	ram_output(payload, sizeof(payload) - 1);

	zassert_mem_equal(ram_tracing, payload, sizeof(payload) - 1,
			  "Payload was not written to the RAM buffer");
}

/**
 * @brief Successive writes are appended in order.
 */
ZTEST(tracing_ram_backend, test_ram_output_append)
{
	ram_backend->api->init();
	ram_output("AAAA", 4);
	ram_output("BBBB", 4);

	zassert_mem_equal(ram_tracing, "AAAABBBB", 8,
			  "Successive writes were not appended in order");
}

/**
 * @brief A write that exactly fills the buffer is accepted, the next one is
 *        dropped and the full condition latches.
 *
 * This exercises the `(pos + length) > RAM_SIZE` boundary and the `buffer_full`
 * latch in the RAM backend.
 */
ZTEST(tracing_ram_backend, test_ram_output_overflow)
{
	uint8_t chunk[RAM_SIZE / 2];

	ram_backend->api->init();

	memset(chunk, 'A', sizeof(chunk));
	ram_output(chunk, sizeof(chunk));

	memset(chunk, 'B', sizeof(chunk));
	/* Brings the write position to exactly RAM_SIZE: must still be stored. */
	ram_output(chunk, sizeof(chunk));

	zassert_equal(ram_tracing[0], 'A', "First chunk missing");
	zassert_equal(ram_tracing[RAM_SIZE / 2], 'B', "Second chunk missing");
	zassert_equal(ram_tracing[RAM_SIZE - 1], 'B', "Buffer not filled to capacity");

	/* Buffer is exactly full now: any further write must be dropped. */
	ram_output("Z", 1);
	zassert_is_null(memchr(ram_tracing, 'Z', RAM_SIZE),
			"Write past capacity was not dropped");

	/* The full condition must latch, so even a tiny write stays dropped. */
	ram_output("Q", 1);
	zassert_is_null(memchr(ram_tracing, 'Q', RAM_SIZE),
			"buffer_full latch did not hold");
}

/**
 * @brief Re-initializing the backend clears the buffer and the full latch.
 */
ZTEST(tracing_ram_backend, test_ram_init_resets)
{
	uint8_t chunk[RAM_SIZE];

	/* Fill and overflow the backend. */
	ram_backend->api->init();
	memset(chunk, 'X', sizeof(chunk));
	ram_output(chunk, sizeof(chunk));
	ram_output("Y", 1);

	/* A fresh init must clear the buffer and let writes through again. */
	ram_backend->api->init();
	for (uint32_t i = 0; i < RAM_SIZE; i++) {
		zassert_equal(ram_tracing[i], 0, "Buffer not cleared by init");
	}

	ram_output("RESET", 5);
	zassert_mem_equal(ram_tracing, "RESET", 5,
			  "Writes rejected after re-init (full latch not cleared)");
}

ZTEST_SUITE(tracing_ram_backend, NULL, setup, NULL, NULL, NULL);
