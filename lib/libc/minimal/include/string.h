/* string.h */

/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_LIB_LIBC_MINIMAL_INCLUDE_STRING_H_
#define ZEPHYR_LIB_LIBC_MINIMAL_INCLUDE_STRING_H_

#include <stddef.h>
#include <bits/restrict.h>

#ifdef __cplusplus
extern "C" {
#endif

extern char  *strcpy(char *d, const char *s);
extern char  *strncpy(char *d, const char *s,
		      size_t n);
extern char  *strchr(const char *s, int c);
extern char  *strrchr(const char *s, int c);
extern size_t strlen(const char *s);
extern int    strcmp(const char *s1, const char *s2);
extern int    strncmp(const char *s1, const char *s2, size_t n);
extern char *strcat(char *dest,
		    const char *src);
extern char  *strncat(char *d, const char *s,
		      size_t n);
extern char *strstr(const char *s, const char *find);

extern int    memcmp(const void *m1, const void *m2, size_t n);
extern void  *memmove(void *d, const void *s, size_t n);
extern void  *memcpy(void *d, const void *s,
		     size_t n);
extern void  *memset(void *buf, int c, size_t n);
extern void  *memchr(const void *s, unsigned char c, size_t n);

#ifdef __cplusplus
}
#endif

#endif  /* ZEPHYR_LIB_LIBC_MINIMAL_INCLUDE_STRING_H_ */
