#ifdef __TINYC__
#include "../roaring_native.c"
#else
#include "../include/roaring_native.h"
#endif

#include <stdio.h>

int main(void) {
    void *set = xroaring_create();
    if (set == NULL) return 1;
    if (!xroaring_add(set, 10) || !xroaring_add(set, 20) ||
        !xroaring_add(set, 30) || !xroaring_add(set, 1000000)) {
        xroaring_destroy(set);
        return 1;
    }
    printf("count: %lld\n", (long long)xroaring_count(set));
    printf("contains 20: %s\n", xroaring_contains(set, 20) ? "true" : "false");
    printf("contains 25: %s\n", xroaring_contains(set, 25) ? "true" : "false");
    printf("min: %lld\n", (long long)xroaring_min(set));
    printf("max: %lld\n", (long long)xroaring_max(set));
    xroaring_remove(set, 20);
    printf("after remove 20, count: %lld\n", (long long)xroaring_count(set));
    puts("roaring example: PASS");
    xroaring_destroy(set);
    return 0;
}
