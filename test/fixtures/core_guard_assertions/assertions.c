#include <__xl_runtime/xruntime_decl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void expect_i64(int64_t actual, int64_t expected)
{
    if (actual != expected) {
        fputs("native integer assertion failed\n", stderr);
        abort();
    }
}

void expect_str(xstrview actual, xstrview expected)
{
    if (actual.Size != expected.Size ||
        (actual.Size && memcmp(actual.Data, expected.Data, actual.Size))) {
        fputs("native exact-length string assertion failed\n", stderr);
        abort();
    }
}
