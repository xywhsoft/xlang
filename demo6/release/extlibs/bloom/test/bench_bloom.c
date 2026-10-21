#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/bloom.h"

enum { KEY_COUNT = 100000, KEY_BYTES = 24, REPEATS = 100 };

static double elapsed_seconds(clock_t start)
{
    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

static void report(const char *name, const char *action, double seconds)
{
    printf("%s %-8s %.1f Mops/s\n", name, action,
        (double)KEY_COUNT * REPEATS /
        (seconds > 0.0 ? seconds : 1.0 / CLOCKS_PER_SEC) / 1000000.0);
}

static int measure(const char *name, int counting, const char *present, const char *absent)
{
    void *filter = counting ? xcounting_bloom_create(KEY_COUNT, 0.01) :
        xbloom_create(KEY_COUNT, 0.01);
    clock_t start;
    int i, round, found = 0, false_positives = 0;
    double seconds;

    if (!filter) return 0;
    seconds = 0.0;
    for (round = 0; round < REPEATS; ++round) {
        /* 每轮从空筛选器开始，避免计数版饱和；清空不计入插入耗时。 */
        if (counting) xcounting_bloom_clear(filter);
        else xbloom_clear(filter);
        start = clock();
        for (i = 0; i < KEY_COUNT; ++i) {
            const char *key = present + (size_t)i * KEY_BYTES;
            if (counting) xcounting_bloom_add(filter, key, -1);
            else xbloom_add(filter, key, -1);
        }
        seconds += elapsed_seconds(start);
    }
    report(name, "add", seconds);

    start = clock();
    for (round = 0; round < REPEATS; ++round) {
        for (i = 0; i < KEY_COUNT; ++i) {
            const char *key = present + (size_t)i * KEY_BYTES;
            found += counting ? xcounting_bloom_contains(filter, key, -1) :
                xbloom_contains(filter, key, -1);
        }
    }
    seconds = elapsed_seconds(start);
    report(name, "hit", seconds);

    start = clock();
    for (round = 0; round < REPEATS; ++round) {
        for (i = 0; i < KEY_COUNT; ++i) {
            const char *key = absent + (size_t)i * KEY_BYTES;
            false_positives += counting ? xcounting_bloom_contains(filter, key, -1) :
                xbloom_contains(filter, key, -1);
        }
    }
    seconds = elapsed_seconds(start);
    report(name, "miss", seconds);
    printf("%s found=%d false_positives=%d/%d\n",
        name, found, false_positives / REPEATS, KEY_COUNT);
    if (counting) xcounting_bloom_destroy(filter);
    else xbloom_destroy(filter);
    return found == KEY_COUNT * REPEATS;
}

int main(void)
{
    char *present = (char *)malloc((size_t)KEY_COUNT * KEY_BYTES);
    char *absent = (char *)malloc((size_t)KEY_COUNT * KEY_BYTES);
    int i, ok;
    if (!present || !absent) { free(present); free(absent); return 1; }
    for (i = 0; i < KEY_COUNT; ++i) {
        snprintf(present + (size_t)i * KEY_BYTES, KEY_BYTES, "in:%08d", i);
        snprintf(absent + (size_t)i * KEY_BYTES, KEY_BYTES, "out:%08d", i);
    }
    ok = measure("bloom", 0, present, absent);
    ok &= measure("counting", 1, present, absent);
    free(present);
    free(absent);
    return ok ? 0 : 1;
}
