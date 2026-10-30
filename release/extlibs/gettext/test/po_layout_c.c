#define XRT_MODULE_ALL
#include "xrt.h"
#include "../gettext_native.h"
#include <stdio.h>
#include <string.h>

static unsigned failures, cases;

static void translated(const char* po, const char* context, const char* id,
    const char* plural, const char* expected)
{
    xvalue* catalog = xgt_parse_po(po);
    const char* result = xrtValueType(catalog) == XVALUE_OBJECT ?
        (plural ? xgt_tn_borrowed(catalog, id, plural, 2) :
            xgt_tc_borrowed(catalog, context, id)) : NULL;
    if (!result || strcmp(result, expected) || xrtGetError()) {
        fprintf(stderr, "PO layout case %u failed\n", cases);
        ++failures;
    }
    xrtValueRelease(catalog);
    ++cases;
}

int main(void)
{
    const char* separators[] = {"", " ", "\t", " \t", "\t ", " \t \t "};
    const char* adjacent = "msgid \"plain\"\nmsgstr \"PLAIN\"\n"
        "msgctxt \"menu\"\nmsgid \"Open\"\nmsgstr \"MENU\"\n"
        "msgctxt \"toolbar\"\nmsgid \"Open\"\nmsgstr \"TOOLBAR\"\n";
    const char* invalid[] = {
        "msgidx \"x\"\nmsgstr \"bad\"\n",
        "msgctxt_extra \"x\"\nmsgid \"x\"\nmsgstr \"bad\"\n",
        "msgid \"x\"\nmsgid_plural_extra \"xs\"\nmsgstr[0] \"bad\"\n",
        "msgid \"missing translation\"\nmsgctxt \"menu\"\nmsgid \"x\"\nmsgstr \"bad\"\n",
        "msgctxt \"menu\"\nmsgctxt \"toolbar\"\nmsgid \"x\"\nmsgstr \"bad\"\n"
    };
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    for (size_t i = 0; i < sizeof(separators) / sizeof(separators[0]); ++i) {
        char po[1024];
        const char* s = separators[i];
        snprintf(po, sizeof(po), "msgctxt%s\"menu\"\nmsgid%s\"Open\"\nmsgstr%s\"MENU\"\n", s, s, s);
        translated(po, "menu", "Open", NULL, "MENU");
        snprintf(po, sizeof(po), "msgid%s\"file\"\nmsgid_plural%s\"files\"\nmsgstr[0]%s\"one\"\nmsgstr[1]%s\"many\"\n", s, s, s, s);
        translated(po, NULL, "file", "files", "many");
    }
    translated(adjacent, "menu", "Open", NULL, "MENU");
    translated(adjacent, "toolbar", "Open", NULL, "TOOLBAR");
    translated("#, fuzzy\nmsgid \"draft\"\nmsgstr \"skip\"\nmsgctxt \"menu\"\nmsgid \"Open\"\nmsgstr \"MENU\"\n",
        "menu", "Open", NULL, "MENU");
    translated("msgid \"empty\"\nmsgstr \"\"\nmsgctxt \"menu\"\nmsgid \"Open\"\nmsgstr \"MENU\"\n",
        "menu", "Open", NULL, "MENU");
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        xvalue* catalog = xgt_parse_po(invalid[i]);
        if (xrtValueType(catalog) != XVALUE_NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) ++failures;
        xrtValueRelease(catalog);
        ++cases;
    }
    /* 新提交边界的每个分配失败点都须退回空结果，不能泄漏半份目录。 */
    bool completed = false, failed = false;
    unsigned budgets = 0;
    for (uint64 budget = 0; budget < 128; ++budget) {
        if (!xrtMemDebugFailAfter(budget)) { ++failures; break; }
        xvalue* catalog = xgt_parse_po(adjacent);
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            if (xrtValueType(catalog) != XVALUE_NULL || xrtErrorKind(xrtGetError()) != XERR_MEMORY) ++failures;
            failed = true;
        } else {
            if (xrtValueType(catalog) != XVALUE_OBJECT || xrtGetError()) ++failures;
            completed = xrtValueType(catalog) == XVALUE_OBJECT;
        }
        xrtValueRelease(catalog);
        ++budgets;
        if (completed || !triggered) break;
    }
    if (!failed || !completed) ++failures;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
        before.InvalidFreeCount != after.InvalidFreeCount || before.DoubleFreeCount != after.DoubleFreeCount ||
        before.OverflowCount != after.OverflowCount || before.UnderflowCount != after.UnderflowCount) ++failures;
    printf("gettext PO layout C: %u cases, %u budgets, %u failures\n", cases, budgets, failures);
    return failures ? 1 : 0;
}

/* 自包含的独立 C 复现，不与 po_layout.xl 形成同名 companion。 */
#include "../gettext_native.c"
#include "../gettext_plural.c"
