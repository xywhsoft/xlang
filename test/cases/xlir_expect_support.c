#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Source fixtures import this assertion boundary. Keep the support TU free of
 * compiler runtime semantics so it remains usable by canonical CLI and
 * generated-C runtime checks. */
void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "XLIR runtime assertion mismatch: %lld != %lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "XLIR runtime string assertion mismatch: %s != %s\n",
                actual != NULL ? actual : "(null)",
                expected != NULL ? expected : "(null)");
        exit(1);
    }
}
