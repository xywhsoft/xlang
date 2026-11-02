/* 空 msgid 只有在没有 msgctxt 时才是元数据，不能豁免普通消息的审核规则。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../gettext_native.h"
#include <stdio.h>
#include <string.h>

static unsigned cases, checks, failures, oom, budgets;

static void check(bool ok, const char* why)
{
    ++checks;
    if (!ok) { ++failures; fprintf(stderr, "gettext PO review: %s\n", why); }
}

static bool text_is(const char* actual, const char* expected)
{
    return actual && !strcmp(actual, expected) && !xrtGetError();
}

static bool balanced(const xmemdebugsnapshot* a, const xmemdebugsnapshot* b)
{
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->OverflowCount == b->OverflowCount && a->UnderflowCount == b->UnderflowCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

static bool recovered(xvalue* catalog)
{
    return xrtValueType(catalog) == XVALUE_OBJECT &&
        text_is(xgt_tc_borrowed(catalog, "menu", ""), "approved") &&
        text_is(xgt_tc_borrowed(catalog, "", ""), "empty context") &&
        text_is(xgt_tn_borrowed(catalog, "file", "files", 5), "third");
}

int main(void)
{
    static const char* contexts[] = {"", "msgctxt \"\"\n", "msgctxt \"menu\"\n"};
    static const char* names[] = {NULL, "", "menu"};
    static const char* ids[] = {"", "Open"};
    static const char* separators[] = {"", "\n", "\r\n"};
    static const char recovery[] =
        "#, fuzzy\nmsgid \"\"\nmsgstr \"Plural-Forms: nplurals=3; plural=n%3;\\n\"\n"
        "#, fuzzy\nmsgctxt \"menu\"\nmsgid \"\"\nmsgstr \"draft\"\n"
        "msgctxt \"menu\"\nmsgid \"\"\nmsgstr \"\"\n"
        "msgctxt \"menu\"\nmsgid \"\"\nmsgstr \"approved\"\n"
        "msgctxt \"\"\nmsgid \"\"\nmsgstr \"\"\n"
        "msgctxt \"\"\nmsgid \"\"\nmsgstr \"empty context\"\n"
        "msgid \"file\"\nmsgid_plural \"files\"\n"
        "msgstr[0] \"first\"\nmsgstr[1] \"second\"\nmsgstr[2] \"third\"\n";
    static const char* invalid[] = {
        "#, fuzzy\nmsgctxt \"\004\"\nmsgid \"\"\nmsgstr \"draft\"\n",
        "#, fuzzy\nmsgctxt \"menu\"\nmsgid \"\"\nmsgstr \"\xff\"\n",
        "#, fuzzy\nmsgctxt \"menu\"\nmsgid \"\"\n"
    };
    xmemdebugsnapshot before, after;
    check(xrtMemDebugEnabled() || xrtMemDebugEnable(true), "enable tracking");
    xrtSetErrorInfo(XERR_VALUE, "gettext.test", 0, "warmup");
    xrtClearError();

    /* 同时覆盖真目录头、显式空上下文、非空上下文，以及空行/相邻条目。 */
    for (unsigned context = 0; context < 3; ++context)
        for (unsigned id = 0; id < 2; ++id)
            for (unsigned state = 0; state < 4; ++state)
                for (unsigned layout = 0; layout < 3; ++layout) {
                    char entry[512], po[1200], key[64];
                    bool header = context == 0 && id == 0;
                    bool skip = !header && state != 0;
                    const char* value = state & 2 ? "" : "approved";
                    const char* expected = skip ? ids[id] : value;
                    xvalue* catalog;
                    char* owned;
                    snprintf(entry, sizeof(entry), "%s%smsgid \"%s\"\nmsgstr \"%s\"\n",
                        state & 1 ? "#, fuzzy\n" : "", contexts[context], ids[id], value);
                    snprintf(po, sizeof(po), "%s%smsgid \"keep\"\nmsgstr \"kept\"\n",
                        entry, separators[layout]);
                    snprintf(key, sizeof(key), "%s%s%s", names[context] ? names[context] : "",
                        context ? "\004" : "", ids[id]);
                    xrtMemDebugSnapshot(&before);
                    catalog = xgt_parse_po(po);
                    check(xrtValueType(catalog) == XVALUE_OBJECT && !xrtGetError(), "parse reviewed entry");
                    check((xrtValueObjectGet(catalog, (xstrview){key, strlen(key)}) != NULL) == !skip,
                        "skipped entry must not occupy a catalog key");
                    check(text_is(context ? xgt_tc_borrowed(catalog, names[context], ids[id]) :
                        xgt_t_borrowed(catalog, ids[id]), expected), "borrowed result");
                    check(text_is(xgt_t_borrowed(catalog, "keep"), "kept"), "flags do not leak to next entry");
                    owned = context ? xgt_tc(catalog, names[context], ids[id]) : xgt_t(catalog, ids[id]);
                    xrtValueRelease(catalog);
                    check(text_is(owned, expected), "owned result survives catalog release");
                    xrtFree(owned);
                    xrtMemDebugSnapshot(&after);
                    check(balanced(&before, &after), "entry memory balance");
                    ++cases;

                    /* 被跳过的条目不占键，已审核译文及真正目录头仍禁止重复。 */
                    snprintf(po, sizeof(po), "%s%s%smsgid \"%s\"\nmsgstr \"replacement\"\n",
                        entry, separators[layout], contexts[context], ids[id]);
                    xrtMemDebugSnapshot(&before);
                    catalog = xgt_parse_po(po);
                    if (skip) check(xrtValueType(catalog) == XVALUE_OBJECT &&
                        text_is(context ? xgt_tc_borrowed(catalog, names[context], ids[id]) :
                            xgt_t_borrowed(catalog, ids[id]), "replacement"), "skipped duplicate may be replaced");
                    else check(xrtValueType(catalog) == XVALUE_NULL &&
                        xrtErrorKind(xrtGetError()) == XERR_VALUE, "active duplicate rejected");
                    xrtValueRelease(catalog);
                    xrtClearError();
                    xrtMemDebugSnapshot(&after);
                    check(balanced(&before, &after), "duplicate memory balance");
                    ++cases;
                }

    /* 审核跳过不能绕过格式验证；坏输入后必须能继续解析。 */
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        xvalue* catalog = xgt_parse_po(invalid[i]);
        check(xrtValueType(catalog) == XVALUE_NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE,
            "malformed draft rejected");
        xrtValueRelease(catalog);
        catalog = xgt_parse_po(recovery);
        check(recovered(catalog), "recovery and fuzzy metadata header");
        xrtValueRelease(catalog);
        ++cases;
    }

    /* 扫描混合目录的每个实际分配失败点，拒绝半份目录和隐性泄漏。 */
    bool completed = false;
    for (uint64 budget = 0; budget < 128; ++budget) {
        xvalue* catalog;
        bool triggered;
        xrtMemDebugSnapshot(&before);
        check(xrtMemDebugFailAfter(budget), "set allocation budget");
        catalog = xgt_parse_po(recovery);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            check(xrtValueType(catalog) == XVALUE_NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
                "OOM kind and no partial catalog");
            ++oom;
        } else { check(recovered(catalog), "allocation scan reaches valid result"); completed = true; }
        xrtValueRelease(catalog);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        check(balanced(&before, &after), "OOM memory balance");
        ++budgets;
        if (completed) break;
    }
    check(completed && oom > 0, "allocation scan completes");
    printf("gettext PO review C: %u cases, %u checks, %u actual OOM/%u budgets, %u failures\n",
        cases, checks, oom, budgets, failures);
    return failures ? 1 : 0;
}

/* 单文件入口可直接 xl run/build；生产包仍分别编译两个实现文件。 */
#include "../gettext_native.c"
#include "../gettext_plural.c"
