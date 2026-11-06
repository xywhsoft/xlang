#define XRT_MODULE_STRING
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/ansi_native.h"
#include <stdio.h>
#include <string.h>

static char* produce(unsigned kind)
{
    switch (kind) {
    case 0: return xansi_bold("x"); case 1: return xansi_dim("x");
    case 2: return xansi_italic("x"); case 3: return xansi_underline("x");
    case 4: return xansi_blink("x"); case 5: return xansi_inverse("x");
    case 6: return xansi_hidden("x"); case 7: return xansi_strike("x");
    case 8: return xansi_fg("x", 31); case 9: return xansi_bg("x", 41);
    case 10: return xansi_fg256("x", 42); case 11: return xansi_bg256("x", 42);
    case 12: return xansi_fg_true("x", 1, 2, 3); case 13: return xansi_bg_true("x", 1, 2, 3);
    case 14: return xansi_title("x"); default: return xansi_strip("x");
    }
}

int main(void)
{
    unsigned checks = 0, failures = 0;
    xmemdebugsnapshot before, after;
    // 同样输入；C借用文字常量，不构造语言字符串/NUL针串，两侧成本分别统计。
    xrtSetErrorInfo(XERR_VALUE, "ansi.test", 0, "warm diagnostic slot");
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (unsigned kind = 0; kind < 16; ++kind) {
        char* expected = produce(kind);
        if (expected == NULL) return 2;
        for (unsigned budget = 0; budget < 2; ++budget) {
            if (!xrtMemDebugFailAfter(budget)) return 2;
            char* value = produce(kind);
            bool injected = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            bool ok = budget == 0 ? !value && injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY :
                value && !injected && !xrtGetError() && strcmp(value, expected) == 0;
            if (!ok) { ++failures; fprintf(stderr, "ANSI input kind=%u budget=%u\n", kind, budget); }
            ++checks;
            xrtFree(value);
            xrtClearError();
        }
        xrtFree(expected);
    }
    if (!xrtMemDebugFailAfter(0)) return 2;
    int64_t width = xansi_display_width("x");
    bool injected = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    if (width != 1 || injected || xrtGetError()) ++failures;
    ++checks;
    xrtMemDebugSnapshot(&after);
    if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
        before.InvalidFreeCount != after.InvalidFreeCount || before.DoubleFreeCount != after.DoubleFreeCount ||
        before.OverflowCount != after.OverflowCount || before.UnderflowCount != after.UnderflowCount) ++failures;
    printf("ansi C input budgets: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}

#ifdef __TINYC__
#include "../ansi_native.c"
#endif
