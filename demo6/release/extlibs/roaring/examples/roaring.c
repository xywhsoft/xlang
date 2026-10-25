#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif

#ifdef __TINYC__
#include "../roaring_native.c"
#else
#include "../include/roaring_native.h"
#endif

#include <stdio.h>
#include "xrt.h"

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
    xvalue *ordered = xroaring_to_array(set);
    if (ordered == NULL || xrtValueType(ordered) != XVALUE_ARRAY ||
        xrtValueCount(ordered) != 3) {
        xroaring_destroy(set);
        return 1;
    }
    {
        int64 first, second, third;
        if (!xrtValueGetInt(xrtValueArrayGet(ordered, 0), &first) ||
            !xrtValueGetInt(xrtValueArrayGet(ordered, 1), &second) ||
            !xrtValueGetInt(xrtValueArrayGet(ordered, 2), &third)) {
            xrtValueRelease(ordered);
            xroaring_destroy(set);
            return 1;
        }
        printf("ordered: %lld %lld %lld\n",
            (long long)first, (long long)second, (long long)third);
    }
    xrtValueRelease(ordered);
    {
        void *other = xroaring_create();
        void *combined = xroaring_create();
        if (other == NULL || combined == NULL ||
            !xroaring_add(other, 30) || !xroaring_add(other, 40) ||
            !xroaring_combine_into(combined, set, other, 0)) {
            xroaring_destroy(combined);
            xroaring_destroy(other);
            xroaring_destroy(set);
            return 1;
        }
        printf("union count: %lld\n", (long long)xroaring_count(combined));
        if (!xroaring_combine_into(combined, set, other, 1)) {
            xroaring_destroy(combined);
            xroaring_destroy(other);
            xroaring_destroy(set);
            return 1;
        }
        printf("intersection count: %lld\n", (long long)xroaring_count(combined));
        if (!xroaring_combine_into(combined, set, other, 2)) {
            xroaring_destroy(combined);
            xroaring_destroy(other);
            xroaring_destroy(set);
            return 1;
        }
        printf("difference count: %lld\n", (long long)xroaring_count(combined));
        xroaring_destroy(combined);
        xroaring_destroy(other);
    }
    puts("roaring example: PASS");
    xroaring_destroy(set);
    return 0;
}
