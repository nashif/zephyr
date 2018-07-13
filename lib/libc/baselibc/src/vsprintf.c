/*
 * vsprintf.c
 */

#include <stdio.h>
#ifndef NO_UNISTD_H
#include <unistd.h>
#endif

int vsprintf(char *buffer, const char *format, va_list ap)
{
	return vsnprintf(buffer, ~(size_t) 0, format, ap);
}
