/*
 * Copyright (c) 2019 Intel corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _TRACE_FORMAT_H
#define _TRACE_FORMAT_H

#include <string.h>
#include <assert.h>
#include <syscall.h>
#include <sys/util.h>
#include <toolchain/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro used internally. This Macro do the buffer copy for the
 *        given parameter. TRACING_DATA will map this Macro to all of the
 *        parameters with the help of MACRO_MAP.
 */
#define _TRACING_APPEND_ERR "please configure larger value for"                \
	STRINGIFY(CONFIG_TRACING_PACKET_BUF_SIZE) "to cover all the parameters"
#define _TRACING_PARAM_APPEND(x)                                               \
	{                                                                      \
		if (length + sizeof(x) <= CONFIG_TRACING_PACKET_BUF_SIZE) {    \
			memcpy(packet_index, &(x), sizeof(x));                 \
			length += sizeof(x);                                   \
			packet_index += sizeof(x);                             \
		} else {                                                       \
			__ASSERT(false, _TRACING_APPEND_ERR);                  \
		}                                                              \
	}

/**
 * @brief Macro to tracing a message in data format. This Macro will copy
 *        all the parameters to the calling thread stack buffer, and at
 *        the same time get the length of the copied buffer, finally the
 *        buffer and length will be given to tracing_format_data.
 */
#define TRACING_DATA(...)                                                 \
	do {                                                              \
		u32_t length = 0;                                         \
		u8_t packet[CONFIG_TRACING_PACKET_BUF_SIZE];              \
		u8_t *packet_index = &packet[0];                          \
									  \
		MACRO_MAP(_TRACING_PARAM_APPEND, ##__VA_ARGS__);          \
		tracing_format_data(packet, length);                      \
	} while (false)

/**
 * @brief Macro to tracing a message in string format.
 */
#define TRACING_STRING(fmt, ...)                                          \
	do {                                                              \
		tracing_format_string(fmt, ##__VA_ARGS__);                \
	} while (false)

/**
 * @brief Tracing a message in string format.
 *
 * @param str String to format.
 * @param ... Variable length arguments.
 */
void tracing_format_string(const char *str, ...);

/* Internal function used by tracing_format_string */
__syscall void z_tracing_format_raw_str(const char *data, u32_t length);

/**
 * @brief Tracing a message in data format.
 *
 * @param data   Data to be traced.
 * @param length Data length.
 */
__syscall void tracing_format_data(const char *data, u32_t length);

#include <syscalls/tracing_format.h>

#ifdef __cplusplus
}
#endif

#endif
