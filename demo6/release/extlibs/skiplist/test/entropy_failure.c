#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_RANDOM
#define XRT_MODULE_RANDOM_SECURE
#include "xrt.h"
#include <stdio.h>

static bool failEntropy = true;
static unsigned entropyCalls;

static bool test_entropy(ptr data, size_t size) {
    entropyCalls++;
    if (!failEntropy) return xrtSecureRandom(data, size);
    xrtSetErrorInfo(XERR_IO, "xlang.extlib.skiplist.test", 0,
        "injected entropy failure");
    return false;
}

#define xrtSecureRandom test_entropy
#include "../skiplist_native.c"
#undef xrtSecureRandom

int main(void) {
    xmemdebugsnapshot before, after;
    void *list;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    if (!before.Enabled) {
        fputs("skiplist memory accounting is disabled\n", stderr);
        return 1;
    }
    list = xskiplist_create();
    if (list != NULL || entropyCalls != 1u ||
        xrtErrorKind(xrtGetError()) != XERR_IO) {
        xskiplist_destroy(list);
        fputs("skiplist entropy failure was not preserved\n", stderr);
        return 1;
    }
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (after.LiveCount != before.LiveCount ||
        after.LiveBytes != before.LiveBytes) {
        fputs("skiplist entropy failure leaked memory\n", stderr);
        return 1;
    }
    failEntropy = false;
    list = xskiplist_create();
    if (list == NULL || entropyCalls != 2u || xrtGetError() != NULL) {
        xskiplist_destroy(list);
        fputs("skiplist did not recover after entropy failure\n", stderr);
        return 1;
    }
    xskiplist_destroy(list);
    xrtMemDebugSnapshot(&after);
    if (after.LiveCount != before.LiveCount ||
        after.LiveBytes != before.LiveBytes) {
        fputs("skiplist recovery leaked memory\n", stderr);
        return 1;
    }
    return 0;
}
