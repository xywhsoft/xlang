/* 原生分配预算与故障回滚；输入为借用 C 字符串，不计语言端的校验成本。 */
#define XRT_MODULE_MAP
#define XRT_MODULE_MUTEX
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/lru_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "LRU budget line %d: %s\n", __LINE__, #value); return 0; \
} } while (0)

static int balanced(const xmemdebugsnapshot* a, const xmemdebugsnapshot* b)
{
    return a->Enabled && b->Enabled && a->LiveCount == b->LiveCount &&
        a->LiveBytes == b->LiveBytes && a->InvalidFreeCount == b->InvalidFreeCount &&
        a->DoubleFreeCount == b->DoubleFreeCount && a->OverflowCount == b->OverflowCount &&
        a->UnderflowCount == b->UnderflowCount;
}

static int operation(void* cache, unsigned kind, char** owned)
{
    if (kind == 0) return xlru_put(cache, "a", "new") != 0;
    if (kind == 1 || kind == 2 || kind == 7) {
        *owned = xlru_get(cache, kind == 2 ? "missing" : "a", kind == 7 ? "" : "fallback");
        return *owned != NULL;
    }
    if (kind == 3) return xlru_contains(cache, "a") == 1;
    if (kind == 4) return xlru_remove(cache, "a") == 1;
    if (kind == 5) return xlru_remove(cache, "missing") == 0;
    return xlru_put(cache, "c", "third") != 0;
}

static int one_case(unsigned mode, unsigned kind, unsigned budget, unsigned required)
{
    xmemdebugsnapshot before, after;
    void* cache;
    char* held;
    char* result = NULL;
    char* current;
    bool injected;
    int succeeded;
    xrtMemDebugSnapshot(&before);
    cache = xlru_create(2, mode);
    CHECK(cache != NULL && xlru_put(cache, "a", "old") && xlru_put(cache, "b", "second"));
    held = xlru_get(cache, "a", "");
    CHECK(held != NULL && strcmp(held, "old") == 0);

    CHECK(xrtMemDebugFailAfter(budget));
    succeeded = operation(cache, kind, &result);
    injected = xrtMemDebugFailTriggered();
    CHECK(injected ? xrtErrorKind(xrtGetError()) == XERR_MEMORY : xrtGetError() == NULL);
    xrtMemDebugFailClear();
    CHECK(succeeded == (budget >= required) && injected == !succeeded);
    xrtClearError();

    CHECK(xlru_hits(cache) == 1 + (succeeded && (kind == 1 || kind == 7)));
    CHECK(xlru_misses(cache) == (succeeded && kind == 2));
    CHECK(xlru_count(cache) == 2 - (succeeded && kind == 4));
    if (succeeded && (kind == 1 || kind == 2 || kind == 7))
        CHECK(strcmp(result, kind == 2 ? "fallback" : "old") == 0);
    if (succeeded && kind == 6) {
        CHECK(xlru_contains(cache, "a") == 1 && xlru_contains(cache, "b") == 0);
        CHECK(xlru_contains(cache, "c") == 1);
    } else {
        CHECK(xlru_contains(cache, "b") == 1 && xlru_contains(cache, "c") == 0);
    }
    if (succeeded && kind == 4) CHECK(xlru_contains(cache, "a") == 0);
    else {
        current = xlru_get(cache, "a", "");
        CHECK(current != NULL && strcmp(current, succeeded && kind == 0 ? "new" : "old") == 0);
        xrtFree(current);
    }
    CHECK(xlru_clear(cache) && xlru_count(cache) == 0 && xlru_hits(cache) == 0 && xlru_misses(cache) == 0);
    xlru_destroy(cache);
    CHECK(strcmp(held, "old") == 0);
    xrtFree(held);
    xrtFree(result);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    CHECK(balanced(&before, &after));
    return 1;
}

int main(void)
{
    static const unsigned costs[] = {1, 1, 1, 0, 0, 0, 4, 1};
    unsigned checks = 0;
    /* 先建立诊断槽，避免把一次性的错误设施初始化计入操作预算。 */
    xrtSetErrorInfo(XERR_VALUE, "lru.test", 0, "warm diagnostic slot");
    xrtClearError();
    for (unsigned mode = 0; mode < 2; ++mode) {
        for (unsigned kind = 0; kind < sizeof(costs) / sizeof(costs[0]); ++kind) {
            for (unsigned budget = 0; budget <= costs[kind]; ++budget) {
                if (!one_case(mode, kind, budget, costs[kind])) {
                    fprintf(stderr, "mode=%u kind=%u budget=%u required=%u\n", mode, kind, budget, costs[kind]);
                    return 1;
                }
                ++checks;
            }
        }
    }
    printf("lru C input budgets: %u checks\n", checks);
    return checks == 32 ? 0 : 1;
}

#ifdef __TINYC__
#include "../lru_native.c"
#endif
