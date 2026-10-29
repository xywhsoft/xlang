/* Standalone C entrypoint, not an automatic xlang companion. */
#include <limits.h>
#include <stdio.h>
#include "xrt.h"
#include "../include/op.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "op delay failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

int main(void) {
    xrtSetErrorInfo(XERR_VALUE, "op.test", 0, "old error");
    xop_delay(0);
    CHECK(xrtGetError() == NULL);
    xop_delay(-1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xop_delay(LLONG_MAX);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xop_delay(0);
    CHECK(xrtGetError() == NULL);
    puts("op delay: PASS");
    return 0;
}
