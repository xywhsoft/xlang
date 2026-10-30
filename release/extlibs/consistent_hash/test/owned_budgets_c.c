/* 同一四组输入的原生读取预算；结果独立持有，销毁节点环后仍可使用。 */
#define XRT_MODULE_HASH64
#define XRT_MODULE_MUTEX
#define XRT_MODULE_STRING
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/consistent_hash_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "consistent_hash budget line %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

static int balanced(xmemdebugsnapshot a, xmemdebugsnapshot b) {
    return a.Enabled && b.Enabled && a.LiveCount == b.LiveCount && a.LiveBytes == b.LiveBytes &&
        a.InvalidFreeCount == b.InvalidFreeCount && a.DoubleFreeCount == b.DoubleFreeCount &&
        a.OverflowCount == b.OverflowCount && a.UnderflowCount == b.UnderflowCount;
}

int main(void) {
    unsigned scenario, budget;
    void *warm = xchash_create(8);
    CHECK(warm != NULL);
    xchash_destroy(warm);
    for (scenario = 0; scenario < 4; ++scenario) {
        for (budget = 0; budget <= 1; ++budget) {
            char name[4097] = "", key[8196] = "", expected[4097];
            char *held;
            void *ring;
            bool triggered;
            size_t i;
            xmemdebugsnapshot before, after;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            if (scenario == 1) { strcpy(name, "node-A"); strcpy(key, "short-key"); }
            if (scenario == 2) {
                for (i = 0; i < 1365; ++i) memcpy(name + i * 3, "\xe4\xbd\xa0", 3);
                name[4095] = 'x'; name[4096] = 0;
            }
            if (scenario > 1) {
                memset(key, 'k', 8192);
                memcpy(key + 8192, "\xe4\xbd\xa0", 4);
            }
            strcpy(expected, name);
            ring = xchash_create(8);
            CHECK(ring != NULL);
            if (name[0]) CHECK(xchash_add_node(ring, name));
            CHECK(xrtMemDebugFailAfter(budget));
            held = xchash_get_node(ring, key);
            triggered = xrtMemDebugFailTriggered();
            CHECK(budget == 0 ? held == NULL && triggered && xrtErrorKind(xrtGetError()) == XERR_MEMORY :
                held != NULL && !triggered && xrtGetError() == NULL);
            xrtMemDebugFailClear();
            xrtClearError();
            CHECK(xchash_node_count(ring) == (name[0] ? 1 : 0));
            if (name[0]) CHECK(xchash_remove_node(ring, name));
            xchash_destroy(ring);
            memset(name, 'R', sizeof(name));
            if (held != NULL) CHECK(strcmp(held, expected) == 0);
            xrtFree(held);
            xrtMemDebugSnapshot(&after);
            CHECK(balanced(before, after));
        }
    }
    puts("consistent_hash C owned budgets: 8 checks, PASS");
    return 0;
}

#ifdef __TINYC__
#include "../consistent_hash_native.c"
#endif
