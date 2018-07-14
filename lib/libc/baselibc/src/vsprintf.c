/*
 * vsprintf.c
 */

#include <stdio.h>
#ifndef NO_UNISTD_H
#include <unistd.h>
#endif
#include <stdint.h>

int vsprintf(char *buffer, const char *format, va_list ap)
{
	return vsnprintf(buffer, SIZE_MAX/2, format, ap);
}
