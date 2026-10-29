/* The two legacy fixtures need only these exact C declarations. Do not import
 * unrelated host functions with platform-dependent long-long prototypes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void expect_i64(int64_t actual, int64_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "time integer expectation failed\n"); exit(1);
    }
}
void expect_str(const char* actual, const char* expected)
{
    if (!actual || !expected || strcmp(actual, expected)) {
        fprintf(stderr, "time string expectation failed\n"); exit(1);
    }
}
