/*
 * Copyright (c) 2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief definition
 *
 * The macro "" is intended to be private to the minimal libc
 * library.  It evaluates to the "restrict" keyword when a C99 compiler is
 * used, and to "__restrict__" when a C++ compiler is used.
 */

#if !defined(_defined)
#define _defined

#ifdef __cplusplus
	#define __restrict__
#else
	#define restrict
#endif

#endif
