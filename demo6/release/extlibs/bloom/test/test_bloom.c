#include <stdio.h>
#include <string.h>

extern void* xbloom_create(long long capacity, double fpRate);
extern void xbloom_destroy(void* filter);
extern void xbloom_add(void* filter, const char* text, long long length);
extern int xbloom_contains(void* filter, const char* text, long long length);
extern const char* xbloom_serialize(void* filter);
extern void* xbloom_deserialize(const char* data);

int main(void)
{
    void* filter = xbloom_create(256, 0.01);
    void* restored;
    const char* serialized;

    if ( filter == NULL ) {
        fputs("test_bloom: create failed\n", stderr);
        return 1;
    }
    xbloom_add(filter, "alpha", -1);
    xbloom_add(filter, "beta", -1);
    serialized = xbloom_serialize(filter);
    restored = xbloom_deserialize(serialized);
    if ( restored == NULL || !xbloom_contains(restored, "alpha", -1) ||
         !xbloom_contains(restored, "beta", -1) ||
         xbloom_deserialize("zzzz") != NULL ) {
        fputs("test_bloom: roundtrip or invalid-data guard failed\n", stderr);
        xbloom_destroy(restored);
        xbloom_destroy(filter);
        return 1;
    }
    xbloom_destroy(restored);
    xbloom_destroy(filter);
    puts("test_bloom: ok");
    return 0;
}
