/* 用独立 C 宿主验证 CSS 受限语法扫描和结果所有权。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/css_native.h"

#include <stdio.h>
#include <string.h>

static int fail(cstr sMessage)
{
    fprintf(stderr, "css C smoke failed: %s\n", sMessage);
    return 1;
}

static bool ownedEquals(void* pResult, cstr sExpected)
{
    bool bSame = pResult != NULL && strcmp((cstr)pResult, sExpected) == 0;
    xrtFree(pResult);
    return bSame;
}

static bool nestedSheet(char* out, size_t capacity, size_t depth)
{
    const char* prefix = "@media all {";
    const char* rule = ".leaf { color: red; }";
    size_t used = 0u;
    size_t i;
    for (i = 0u; i < depth; ++i) {
        size_t size = strlen(prefix);
        if (size >= capacity - used) return false;
        memcpy(out + used, prefix, size);
        used += size;
    }
    if (strlen(rule) + depth >= capacity - used) return false;
    memcpy(out + used, rule, strlen(rule));
    used += strlen(rule);
    for (i = 0u; i < depth; ++i) out[used++] = '}';
    out[used] = 0;
    return true;
}

static int nestingBoundary(void)
{
    char sheet[2048];
    if (!nestedSheet(sheet, sizeof(sheet), 64u) || !xcss_validate(sheet) ||
        xcss_count_rules(sheet) != 1 ||
        !ownedEquals(xcss_get(sheet, ".leaf", "color"), "red"))
        return fail("64 nested at-rules must remain valid");
    if (!nestedSheet(sheet, sizeof(sheet), 65u) || xcss_validate(sheet) ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_RANGE)
        return fail("65 nested at-rules must report a range error");
    {
        void* result = xcss_get(sheet, ".leaf", "color");
        if (result != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_RANGE) {
            xrtFree(result);
            return fail("get hid the nesting limit");
        }
        result = xcss_selector_at(sheet, 0);
        if (result != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_RANGE) {
            xrtFree(result);
            return fail("selectorAt hid the nesting limit");
        }
    }
    if (xcss_count_rules(sheet) != -1 || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_RANGE ||
        xcss_count_props(sheet, ".leaf") != -1 || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_RANGE)
        return fail("counting hid the nesting limit");
    return 0;
}

/* 变异合法嵌套样本，要求三个公开入口对整份语法给出一致判断。 */
static int mutationBoundary(void)
{
    static const char original[] =
        "@media all { .card { content: \"{;}\"; color: rgb(1, 2, 3); } }";
    static const unsigned char replacements[] = {
        0, '{', '}', ':', ';', '/', '*', '"', 0x80, 0xff
    };
    char sheet[sizeof(original)];
    size_t i, j;

    for (i = 0; i < sizeof(original) - 1u; ++i) {
        for (j = 0; j < sizeof(replacements); ++j) {
            bool valid;
            int64_t count;
            void* result;
            memcpy(sheet, original, sizeof(sheet));
            sheet[i] = (char)replacements[j];
            valid = xcss_validate(sheet);
            if ((valid && xrtGetError() != NULL) ||
                (!valid && xrtGetError() == NULL))
                return fail("mutated validate result lost its error status");
            count = xcss_count_rules(sheet);
            if ((valid && (count < 0 || xrtGetError() != NULL)) ||
                (!valid && (count >= 0 || xrtGetError() == NULL)))
                return fail("mutated countRules disagrees with validate");
            result = xcss_get(sheet, ".card", "color");
            if ((valid && (result == NULL || xrtGetError() != NULL)) ||
                (!valid && (result != NULL || xrtGetError() == NULL))) {
                xrtFree(result);
                return fail("mutated get disagrees with validate");
            }
            xrtFree(result);
        }
    }
    return 0;
}

static int utf8Boundary(void)
{
    const char* invalid = "a { color: red; } /* \xff */";
    const char* invalid_selector = ".\xff";
    const char* invalid_property = "color\xff";
    void* result;

    if (xcss_validate(invalid) || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE)
        return fail("validate accepted invalid UTF-8 in a comment");
    if (xcss_count_rules(invalid) != -1 || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE)
        return fail("countRules accepted invalid UTF-8");
    if (xcss_count_props(invalid, "a") != -1 || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE)
        return fail("countProps accepted invalid UTF-8");
    result = xcss_get(invalid, "a", "color");
    if (result != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        xrtFree(result);
        return fail("get accepted invalid UTF-8 in a later comment");
    }
    result = xcss_selector_at(invalid, 0);
    if (result != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        xrtFree(result);
        return fail("selectorAt accepted invalid UTF-8");
    }
    result = xcss_strip_comments(invalid);
    if (result != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        xrtFree(result);
        return fail("stripComments discarded invalid UTF-8 without error");
    }
    result = xcss_get("a { color: red; }", invalid_selector, "color");
    if (result != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        xrtFree(result);
        return fail("get accepted invalid UTF-8 selector");
    }
    result = xcss_get("a { color: red; }", "a", invalid_property);
    if (result != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        xrtFree(result);
        return fail("get accepted invalid UTF-8 property");
    }
    if (!xcss_validate("段落 { 内容: '你好'; }") ||
        !ownedEquals(xcss_get("段落 { 内容: '你好'; }", "段落", "内容"), "'你好'") ||
        xrtGetError() != NULL)
        return fail("valid UTF-8 CSS query regressed");
    return 0;
}

static int allocationFailures(void)
{
    cstr sCss = "/* head */ .card { color: red; }";
    for (int mode = 0; mode < 3; ++mode) {
        bool failed = false;
        for (uint64 i = 0u; i < 64u; ++i) {
            void* pResult;
            bool triggered;
            xrtClearError();
            if (!xrtMemDebugFailAfter(i)) return fail("allocation injection unavailable");
            pResult = mode == 0 ? xcss_get(sCss, ".card", "color") :
                mode == 1 ? xcss_selector_at(sCss, 0) :
                xcss_strip_comments(sCss);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (!triggered) {
                if (pResult == NULL || xrtGetError() != NULL) {
                    xrtFree(pResult);
                    return fail("text result did not recover after allocation failure");
                }
                xrtFree(pResult);
                break;
            }
            if (pResult != NULL || xrtGetError() == NULL ||
                xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
                xrtFree(pResult);
                return fail("text allocation failure was hidden");
            }
            failed = true;
            if (i == 63u) return fail("allocation scan did not terminate");
        }
        if (!failed) return fail("text operation had no injected allocation");
    }
    xrtClearError();
    return 0;
}

int main(void)
{
    cstr sCss = "/* header */ @font-face { font-family: 'demo'; } "
        "@media screen { .card, .panel { content: \"{;}\"; color: rgb(1, 2, 3); } }";
    void* pHeld = xcss_get(sCss, ".card", "content");
    const char* sPrefix = "p { content: ";
    const char* sSuffix = "; }";
    size_t iPrefix = strlen(sPrefix);
    size_t iSuffix = strlen(sSuffix);
    char* pLarge = (char*)xrtMalloc(iPrefix + 12000u + iSuffix + 1u);
    void* pLargeResult;
    int i;

    if (pHeld == NULL || pLarge == NULL) {
        xrtFree(pHeld);
        xrtFree(pLarge);
        return fail("fixture allocation");
    }
    for (i = 0; i < 12; ++i) {
        if (!ownedEquals(xcss_get(sCss, ".panel", "color"), "rgb(1, 2, 3)")) {
            xrtFree(pHeld);
            xrtFree(pLarge);
            return fail("repeated owned result");
        }
    }
    if (strcmp((cstr)pHeld, "\"{;}\"") != 0) {
        xrtFree(pHeld);
        xrtFree(pLarge);
        return fail("held result changed after later queries");
    }
    xrtFree(pHeld);
    memcpy(pLarge, sPrefix, iPrefix);
    memset(pLarge + iPrefix, 'x', 12000u);
    memcpy(pLarge + iPrefix + 12000u, sSuffix, iSuffix + 1u);
    pLargeResult = xcss_get(pLarge, "p", "content");
    if (pLargeResult == NULL || strlen((cstr)pLargeResult) != 12000u ||
        ((cstr)pLargeResult)[11999] != 'x') {
        xrtFree(pLargeResult);
        xrtFree(pLarge);
        return fail("large owned result");
    }
    xrtFree(pLargeResult);
    xrtFree(pLarge);

    if (!xcss_validate(sCss) || xcss_count_rules(sCss) != 1 ||
        xcss_count_props(sCss, ".panel") != 2 ||
        !ownedEquals(xcss_selector_at(sCss, 0), ".card, .panel") ||
        !ownedEquals(xcss_get(sCss, ".card", "content"), "\"{;}\"") ||
        !ownedEquals(xcss_get(sCss, ".panel", "color"), "rgb(1, 2, 3)") ||
        !ownedEquals(xcss_strip_comments("a/*x*/b"), "a b")) {
        return fail("syntax aware scanner");
    }
    if (!xcss_validate("a/* selector */ { /* before */ color/* name */: red; }") ||
        xcss_count_props("a/* selector */ { color/* name */: red; }", "a") != 1 ||
        !ownedEquals(xcss_get("a/* selector */ { color/* name */: red; }",
            "a", "color"), "red") ||
        !ownedEquals(xcss_get("a { co/**/lor: blue; }", "a", "color"), "")) {
        return fail("comments around selector or property name");
    }
    if (xcss_validate("a { color: red; /* missing") ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("malformed CSS lost its value error");
    }
    xrtClearError();
    if (xcss_validate("a { color red; }") ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("malformed declaration passed validation");
    }
    xrtClearError();
    if (xcss_validate("a { /* no property */: red; }") ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("comment-only declaration name passed validation");
    }
    xrtClearError();
    if (xcss_validate("@font-face { font-family demo; }") ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("malformed at-rule declaration passed validation");
    }
    if (xcss_validate("a { content: 'line\nnext'; }") ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("unescaped newline inside CSS string was accepted");
    }
    if (xcss_validate("a { content: 'line\rnext'; }") ||
        xcss_validate("a { content: 'line\fnext'; }")) {
        return fail("unescaped CR or FF inside CSS string was accepted");
    }
    if (!xcss_validate("a { content: 'line\\\nnext'; }") ||
        !xcss_validate("a { content: 'line\\\r\nnext'; }") ||
        !xcss_validate("a { content: 'line\\\fnext'; }")) {
        return fail("escaped CSS line continuation was rejected");
    }
    {
        void* result = xcss_get("a { color: red; } b { content: 'bad\nline'; }",
            "a", "color");
        if (result != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            xrtFree(result);
            return fail("get ignored a bad string in a later rule");
        }
        result = xcss_strip_comments("a { content: 'bad\nline'; }");
        if (result != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            xrtFree(result);
            return fail("stripComments accepted a bad string");
        }
    }
    xrtClearError();
    if (xcss_selector_at(sCss, -1) != NULL ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_RANGE) {
        return fail("negative index did not report range error");
    }
    if (!ownedEquals(xcss_get(sCss, ".card", "content"), "\"{;}\"") ||
        xrtGetError() != NULL) {
        return fail("a prior error poisoned a successful query");
    }
    {
        cstr sMalformedTail = "a { color: red; } b { color blue; }";
        void* pResult = xcss_get(sMalformedTail, "a", "color");
        if (pResult != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            xrtFree(pResult);
            return fail("get ignored a malformed later rule");
        }
        xrtClearError();
        pResult = xcss_selector_at(sMalformedTail, 0);
        if (pResult != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            xrtFree(pResult);
            return fail("selectorAt ignored a malformed later rule");
        }
        xrtClearError();
        if (xcss_count_rules(sMalformedTail) != -1 || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            return fail("countRules ignored a malformed declaration");
        }
    }
    if (nestingBoundary() || mutationBoundary() || utf8Boundary() ||
        allocationFailures()) return 1;
    puts("css C smoke: PASS");
    return 0;
}
