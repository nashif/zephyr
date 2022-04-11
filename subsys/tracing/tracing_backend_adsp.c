/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <kernel.h>
#include <string.h>
#include <tracing_core.h>
#include <tracing_buffer.h>
#include <tracing_backend.h>


void intel_adsp_trace_out(int8_t *str, size_t len);

static void tracing_backend_adsp_output(
		const struct tracing_backend *backend,
		uint8_t *data, uint32_t length)
{
	intel_adsp_trace_out(data, length);
}

static void tracing_backend_adsp_init(void)
{
	;
}

const struct tracing_backend_api tracing_backend_adsp_api = {
	.init = tracing_backend_adsp_init,
	.output  = tracing_backend_adsp_output
};

TRACING_BACKEND_DEFINE(tracing_backend_adsp, tracing_backend_adsp_api);
