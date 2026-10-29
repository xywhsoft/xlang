#ifdef __TINYC__
#include "../cuckoo_native.c"
#else
#include "../include/cuckoo_native.h"
#include "xrt.h"
#endif

#include <stdio.h>
#include <string.h>

int main(void) {
    void *filter = xcuckoo_create(10000);
    const char *keys[] = {"session:abc123", "session:def456", "session:ghi789"};
    size_t i;
    if (filter == NULL) return 1;
    printf("buckets: %lld\n", (long long)xcuckoo_buckets(filter));
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        if (!xcuckoo_add(filter, keys[i], strlen(keys[i]))) {
            xcuckoo_destroy(filter);
            return 1;
        }
    }
    printf("contains abc123: %s\n", xcuckoo_contains(filter, keys[0], strlen(keys[0])) ? "true" : "false");
    printf("contains zzz999: %s\n", xcuckoo_contains(filter, "session:zzz999", 14) ? "true" : "false");
    if (!xcuckoo_remove(filter, keys[1], strlen(keys[1]))) {
        xcuckoo_destroy(filter);
        return 1;
    }
    printf("after remove def456: %s\n", xcuckoo_contains(filter, keys[1], strlen(keys[1])) ? "true" : "false");
    printf("count: %lld\n", (long long)xcuckoo_count(filter));
    xcuckoo_clear(filter);
    printf("after clear count: %lld\n", (long long)xcuckoo_count(filter));
    printf("after clear abc123: %s\n", xcuckoo_contains(filter, keys[0], strlen(keys[0])) ? "true" : "false");
    if (!xcuckoo_add(filter, "session:reused", 14)) {
        xcuckoo_destroy(filter);
        return 1;
    }
    printf("after reuse: %s\n", xcuckoo_contains(filter, "session:reused", 14) ? "true" : "false");
    xcuckoo_destroy(filter);
    puts("cuckoo example: PASS");
    return 0;
}
