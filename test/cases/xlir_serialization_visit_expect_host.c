#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exact cross-platform ABI for the legacy JSON stream assertions. Do not use
 * long long aliases for XRT's int64_t on LP64 targets or unrelated host symbols. */
void expect_i64(int64_t actual, int64_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "visit expect_i64: actual=%lld expected=%lld\n",
            (long long)actual, (long long)expected);
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fputs("visit expect_str mismatch\n", stderr);
        exit(1);
    }
}
