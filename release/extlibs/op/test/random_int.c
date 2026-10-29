#include <limits.h>
#include <stdio.h>
#include "xrt.h"
#include "../include/op.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "op random int failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

int main(void) {
    int i;
    CHECK(xop_random_int(7, 7) == 7 && xrtGetError() == NULL);
    CHECK(xop_random_int(4, 3) == 0 && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xop_random_int(2, 2) == 2 && xrtGetError() == NULL);
    for (i = 0; i < 1000; ++i) {
        long long value = xop_random_int(0, UINT_MAX);
        CHECK(value >= 0 && value <= UINT_MAX && xrtGetError() == NULL);
        value = xop_random_int(LLONG_MAX - 10, LLONG_MAX);
        CHECK(value >= LLONG_MAX - 10 && value <= LLONG_MAX &&
            xrtGetError() == NULL);
        (void)xop_random_int(LLONG_MIN, LLONG_MAX);
        CHECK(xrtGetError() == NULL);
    }
    puts("op random int: PASS");
    return 0;
}
