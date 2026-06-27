/*
 * Copyright (c) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <string.h>
#include <stdarg.h>
#include <tracing_core.h>
#include <tracing_buffer.h>
#include <tracing_format_common.h>

/*
 * These tests exercise the low-level tracing buffer "put" helpers directly.
 * Returning false from these helpers is exactly what makes the producers in
 * tracing_format_{sync,async}.c invoke tracing_packet_drop_handle(), so the
 * failure branches asserted here are the buffer-overflow / packet-drop path.
 *
 * The global tracing pipeline is disabled for the whole suite so that the only
 * accesses to the shared ring buffer are the ones issued by the test.
 */

static uint8_t scratch[256];

static bool string_put(const char *fmt, ...)
{
	va_list ap;
	bool ret;

	va_start(ap, fmt);
	ret = tracing_format_string_put(fmt, ap);
	va_end(ap);

	return ret;
}

static void *setup(void)
{
	uint8_t cmd[] = "disable";

	tracing_cmd_handle(cmd, sizeof(cmd));
	zassert_false(is_tracing_enabled(), "Failed to disable tracing pipeline");

	return NULL;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	tracing_buffer_init();
}

/**
 * @brief Raw data fitting the free space is accepted, oversized data is dropped.
 */
ZTEST(tracing_buffer, test_raw_data_put_overflow)
{
	uint32_t space = tracing_buffer_space_get();

	memset(scratch, 'a', sizeof(scratch));
	zassert_true(space < sizeof(scratch), "test buffer too small for config");

	zassert_true(tracing_format_raw_data_put(scratch, space),
		     "Raw data exactly filling the buffer should be accepted");

	tracing_buffer_init();
	zassert_false(tracing_format_raw_data_put(scratch, space + 1),
		      "Raw data larger than free space must be dropped");
}

/**
 * @brief tracing_data is stored and read back unchanged.
 */
ZTEST(tracing_buffer, test_data_put_readback)
{
	uint8_t pat[32];
	uint8_t out[32];
	tracing_data_t d = { .data = pat, .length = sizeof(pat) };

	for (uint32_t i = 0; i < sizeof(pat); i++) {
		pat[i] = (uint8_t)(i + 1);
	}

	zassert_true(tracing_format_data_put(&d, 1), "data put should succeed");
	zassert_equal(tracing_buffer_get(out, sizeof(out)), sizeof(out),
		      "Wrong number of bytes read back");
	zassert_mem_equal(out, pat, sizeof(pat), "Stored data does not match");
}

/**
 * @brief A multi-segment tracing_data array is concatenated in order.
 */
ZTEST(tracing_buffer, test_data_put_multi_segment)
{
	uint8_t a[10];
	uint8_t b[20];
	uint8_t out[30];
	tracing_data_t segs[2] = {
		{ .data = a, .length = sizeof(a) },
		{ .data = b, .length = sizeof(b) },
	};

	memset(a, 'A', sizeof(a));
	memset(b, 'B', sizeof(b));

	zassert_true(tracing_format_data_put(segs, 2),
		     "multi-segment data put should succeed");
	zassert_equal(tracing_buffer_get(out, sizeof(out)), sizeof(out),
		      "Wrong number of bytes read back");
	zassert_mem_equal(out, a, sizeof(a), "First segment mismatch");
	zassert_mem_equal(out + sizeof(a), b, sizeof(b), "Second segment mismatch");
}

/**
 * @brief tracing_data that does not fit the buffer is dropped.
 */
ZTEST(tracing_buffer, test_data_put_overflow)
{
	uint8_t payload[8];
	tracing_data_t d = { .data = payload, .length = sizeof(payload) };

	/* Fill the ring buffer completely. */
	memset(scratch, 'f', sizeof(scratch));
	tracing_buffer_put(scratch, tracing_buffer_space_get());
	zassert_equal(tracing_buffer_space_get(), 0, "Buffer should be full");

	zassert_false(tracing_format_data_put(&d, 1),
		      "data put must fail when the buffer is full");
}

/**
 * @brief tracing_data is reassembled correctly when the put wraps the ring.
 *
 * Advancing the read/write cursors near the end of the ring forces
 * tracing_format_data_put() through its partial-claim do/while loop.
 */
ZTEST(tracing_buffer, test_data_put_wraparound)
{
	uint32_t cap = tracing_buffer_capacity_get();
	uint32_t n = (cap * 3) / 4;
	uint32_t m = cap / 2;
	uint8_t pat[256];
	uint8_t out[256];
	tracing_data_t d = { .data = pat, .length = m };

	zassert_true(m > (cap - n), "test sizes do not force a wrap");

	/* Move head/tail near the end of the ring, leaving it empty. */
	memset(scratch, 0, sizeof(scratch));
	zassert_equal(tracing_buffer_put(scratch, n), n, "seed put failed");
	zassert_equal(tracing_buffer_get(scratch, n), n, "seed drain failed");

	for (uint32_t i = 0; i < m; i++) {
		pat[i] = (uint8_t)(i * 7 + 1);
	}

	zassert_true(tracing_format_data_put(&d, 1),
		     "wrapping data put should succeed");
	zassert_equal(tracing_buffer_get(out, m), m, "Wrong byte count read back");
	zassert_mem_equal(out, pat, m, "Data corrupted across ring wrap");
}

/**
 * @brief String formatting succeeds on free space and is dropped when full.
 */
ZTEST(tracing_buffer, test_string_put_overflow)
{
	uint8_t out[8];

	zassert_true(string_put("hello"), "string put should succeed");
	zassert_equal(tracing_buffer_get(out, 5), 5, "Wrong byte count read back");
	zassert_mem_equal(out, "hello", 5, "Formatted string mismatch");

	/* Fill the buffer, then the next string must be dropped. */
	tracing_buffer_init();
	memset(scratch, 'f', sizeof(scratch));
	tracing_buffer_put(scratch, tracing_buffer_space_get());

	zassert_false(string_put("hello"),
		      "string put must fail when the buffer is full");
}

ZTEST_SUITE(tracing_buffer, NULL, setup, before, NULL, NULL);
