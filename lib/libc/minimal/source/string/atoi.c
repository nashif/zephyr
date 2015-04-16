/* From: http://fossies.org/dox/musl-1.0.5/atoi_8c_source.html */

#include <stdlib.h>

int isspace(int c) {
	return c == ' ' || (unsigned)c-'\t' < 5;
}

int isdigit(int a) {
	return (((unsigned)(a)-'0') < 10);
}

int atoi(const char *s) {
	int n=0, neg=0;
	while (isspace(*s)) s++;
	switch (*s) {
	case '-': neg=1;
	case '+': s++;
	}
	/* Compute n as a negative number to avoid overflow on INT_MIN */
	while (isdigit(*s))
		n = 10*n - (*s++ - '0');
	return neg ? n : -n;
}
