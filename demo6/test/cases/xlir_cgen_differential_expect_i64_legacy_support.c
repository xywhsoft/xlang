#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Legacy complete-program C owns main, but the source fixture intentionally
 * imports this assertion boundary.  Keep the support TU free of compiler
 * runtime semantics so both backends still execute the same XL program. */
void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "XLIR differential mismatch: %lld != %lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

void expect_str(char* actual, char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "XLIR differential string mismatch: %s != %s\n",
                actual != NULL ? actual : "(null)",
                expected != NULL ? expected : "(null)");
        exit(1);
    }
}
