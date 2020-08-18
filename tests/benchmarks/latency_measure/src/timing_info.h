/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _TIMING_INFO_H_
#define _TIMING_INFO_H_

#include <timestamp.h>

static inline uint32_t TIMING_INFO_GET_DELTA(uint32_t start, uint32_t end)
{
	return (end >= start) ? (end - start) : (ULONG_MAX - start + end);
}

#endif /* _TIMING_INFO_H_ */
