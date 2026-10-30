#ifndef XL_TEST_NUMERIC_ARG_H
#define XL_TEST_NUMERIC_ARG_H
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
/* Reject missing, partial and overflowing command-line inputs in native probes. */
static inline int xlTestIntArg(const char* text)
{
    char* end;
    long value;
    if (!text || !*text) exit(2);
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || *end || value < INT_MIN || value > INT_MAX) exit(2);
    return (int)value;
}
#endif
