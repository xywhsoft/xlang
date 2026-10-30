/* 连续正文、转义和续行必须保持相同字节及所有权；基准与验收分开运行。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../gettext_native.h"
#include <stdio.h>
#include <string.h>

static const struct { const char *po, *text; } patterns[] = {
    {"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-", "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-"},
    {"\xe4\xbd\xa0\xe5\xa5\xbd", "\xe4\xbd\xa0\xe5\xa5\xbd"},
    {"raw\\n\\t\\\"\\\\\\101", "raw\n\t\"\\A"},
    {"\\n\\t\\r\\a\\b\\f\\v\\?\\\"\\\\", "\n\t\r\a\b\f\v?\"\\"},
    {"\\xE4\\275\\240", "\xe4\xbd\xa0"},
    {"x", "x"}
};
static unsigned failures, cases, oom;

static void check(bool ok, const char *why) {
    if (!ok) { ++failures; fprintf(stderr, "gettext PO spans: %s\n", why); }
}

static bool append(xbuffer *out, const char *text) {
    return xrtBufferAppend(out, (xbytesview){(cbytes)text, strlen(text)});
}

static bool make(xbuffer *po, xbuffer *expected, unsigned pattern, unsigned count, bool multiline) {
    xbuffer body;
    unsigned i;
    if (!xrtBufferInit(&body)) return false;
    if (!append(po, "msgid \"plain\"\nmsgstr \"")) goto fail;
    for (i = 0; i < count; ++i) {
        if (multiline && i && i % 17 == 0 && !append(&body, "\"\n\"")) goto fail;
        if (!append(&body, patterns[pattern].po) || !append(expected, patterns[pattern].text)) goto fail;
    }
    if (!xrtBufferAppend(po, xrtBufferView(&body)) ||
        !append(po, "\"\nmsgctxt \"menu\"\nmsgid \"Open\"\nmsgstr \"saved\"\n") ||
        !append(po, "msgid \"file\"\nmsgid_plural \"files\"\nmsgstr[0] \"") ||
        !xrtBufferAppend(po, xrtBufferView(&body)) ||
        !append(po, "\"\nmsgstr[1] \"many\"\n") || !xrtBufferAppendByte(po, 0)) goto fail;
    xrtBufferUnit(&body);
    return true;
fail:
    xrtBufferUnit(&body);
    return false;
}

static bool same(const char *text, xbytesview expected, const char *empty) {
    return text && (expected.Size ? strlen(text) == expected.Size && !memcmp(text, expected.Data, expected.Size) :
        strcmp(text, empty) == 0);
}

static bool correct(xvalue *catalog, xbytesview expected) {
    const char *context, *plural;
    if (xrtValueType(catalog) != XVALUE_OBJECT) return false;
    context = xgt_tc_borrowed(catalog, "menu", "Open");
    plural = xgt_tn_borrowed(catalog, "file", "files", 2);
    return context && !strcmp(context, "saved") &&
        same(xgt_t_borrowed(catalog, "plain"), expected, "plain") &&
        same(xgt_tn_borrowed(catalog, "file", "files", 1), expected, "file") &&
        plural && !strcmp(plural, "many") && !xrtGetError();
}

static bool balanced(const xmemdebugsnapshot *a, const xmemdebugsnapshot *b) {
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->OverflowCount == b->OverflowCount && a->UnderflowCount == b->UnderflowCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

static int vectors(void) {
    static const unsigned counts[] = {0, 1, 2, 63, 64, 65, 127, 128, 129, 255, 256, 257, 4096};
    unsigned pattern, length, multiline;
    xmemdebugsnapshot before, after;
    check(xrtMemDebugEnabled() || xrtMemDebugEnable(true), "enable tracking");
    xrtSetErrorInfo(XERR_VALUE, "gettext.test", 0, "warmup");
    xrtClearError();
    for (pattern = 0; pattern < 6; ++pattern) for (length = 0; length < sizeof(counts) / sizeof(counts[0]); ++length)
        for (multiline = 0; multiline < 2; ++multiline) {
            xbuffer po, expected;
            xvalue *catalog;
            char *owned;
            xrtMemDebugSnapshot(&before);
            if (!xrtBufferInit(&po) || !xrtBufferInit(&expected) ||
                !make(&po, &expected, pattern, counts[length], multiline != 0)) return 1;
            catalog = xgt_parse_po((const char *)po.Data);
            xrtBufferUnit(&po);
            check(correct(catalog, xrtBufferView(&expected)), "translation or borrowed result mismatch");
            owned = xgt_t(catalog, "plain");
            xrtValueRelease(catalog);
            check(same(owned, xrtBufferView(&expected), "plain"), "owned result did not survive source/catalog release");
            xrtFree(owned);
            xrtBufferUnit(&expected);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            check(balanced(&before, &after), "vector memory imbalance");
            ++cases;
        }
    for (pattern = 0; pattern < 4; ++pattern) {
        xbuffer po, expected;
        bool complete = false;
        unsigned budget;
        if (!xrtBufferInit(&po) || !xrtBufferInit(&expected) || !make(&po, &expected, pattern, 257, true)) return 1;
        for (budget = 0; budget < 128; ++budget) {
            xvalue *catalog;
            bool triggered;
            xrtMemDebugSnapshot(&before);
            check(xrtMemDebugFailAfter(budget), "set OOM");
            catalog = xgt_parse_po((const char *)po.Data);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                check(xrtValueType(catalog) == XVALUE_NULL && xrtGetError() &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY, "OOM returned partial catalog or wrong error");
                ++oom;
            } else { check(correct(catalog, xrtBufferView(&expected)), "OOM recovery mismatch"); complete = true; }
            xrtValueRelease(catalog);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            check(balanced(&before, &after), "OOM memory imbalance");
            if (complete) break;
        }
        check(complete, "OOM scan did not reach successful parse");
        xrtBufferUnit(&po);
        xrtBufferUnit(&expected);
    }
    printf("gettext PO spans: %u cases, %u real OOM, %u failures\n", cases, oom, failures);
    return failures ? 1 : 0;
}

/* 同一编译器/运行库与工作量比较新旧 native.c，不把本机结果当作 ABI 承诺。 */
static int benchmark(void) {
    unsigned pattern, sample, i;
    for (pattern = 0; pattern < 4; ++pattern) {
        xbuffer po, expected;
        if (!xrtBufferInit(&po) || !xrtBufferInit(&expected) || !make(&po, &expected, pattern, 4096, false)) return 1;
        for (sample = 0; sample < 6; ++sample) {
            uint64 start = xrtClock();
            size_t checksum = 0;
            for (i = 0; i < 80; ++i) {
                xvalue *catalog = xgt_parse_po((const char *)po.Data);
                xvalue *value = xrtValueObjectGet(catalog, (xstrview){"plain", 5});
                xstrview text;
                if (!value || !xrtValueGetString(value, &text) || text.Size != expected.Size ||
                    memcmp(text.Data, expected.Data, text.Size)) return 1;
                checksum += text.Size;
                xrtValueRelease(catalog);
            }
            uint64 elapsed = xrtClock() - start;
            if (sample) printf("%u,%u,%llu,%llu\n", pattern, sample,
                (unsigned long long)elapsed, (unsigned long long)checksum);
        }
        xrtBufferUnit(&po);
        xrtBufferUnit(&expected);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--bench")) return benchmark();
    if (argc != 1) return 2;
    return vectors();
}

#ifndef XGT_SPANS_EXTERNAL
#include "../gettext_native.c"
#include "../gettext_plural.c"
#endif
