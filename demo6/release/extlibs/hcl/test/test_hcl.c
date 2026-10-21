/* HCL C smoke: same syntax-aware restricted parser as the xlang package. */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/hcl_native.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <locale.h>

static int fail(cstr sMessage)
{
    fprintf(stderr, "hcl C smoke failed: %s\n", sMessage);
    return 1;
}

static bool ownedEquals(void* pResult, cstr sExpected)
{
    bool bSame = pResult != NULL && strcmp((cstr)pResult, sExpected) == 0;
    xrtFree(pResult);
    return bSame;
}

// 逐点注入分配失败，确认公开查询不泄漏半成品且错误可与语法错误区分。
static void* queryText(unsigned int operation)
{
    cstr sHcl = "name = \"a\\u4F60b\"\nresource \"web\" { enabled = true }\n";

    switch (operation) {
        case 0: return xhcl_get(sHcl, "name");
        case 1: return xhcl_get_raw(sHcl, "name");
        case 2: return xhcl_get(sHcl, "missing");
        case 3: return xhcl_block_label(sHcl, "resource", 0);
        default: return xhcl_block_label_at(sHcl, "resource", 0, 0);
    }
}

static int testAllocationFailures(void)
{
    unsigned int operation;

    for (operation = 0; operation < 5u; ++operation) {
        unsigned int failure;
        bool bFailed = false;
        bool bCompleted = false;

        for (failure = 0; failure < 64u; ++failure) {
            void* pResult;
            bool bTriggered;

            if (!xrtMemDebugFailAfter(failure)) return fail("fault injection setup");
            pResult = queryText(operation);
            bTriggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (bTriggered) {
                bFailed = true;
                if (pResult != NULL || xrtGetError() == NULL ||
                    xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
                    xrtFree(pResult);
                    return fail("allocation failure result or error kind");
                }
            } else {
                if (pResult == NULL || xrtGetError() != NULL) {
                    xrtFree(pResult);
                    return fail("successful query after allocation faults");
                }
                bCompleted = true;
            }
            xrtFree(pResult);
            if (bCompleted) break;
        }
        if (!bFailed || !bCompleted) return fail("allocation faults did not recover");
        // 再调用一次，确认错误状态和分配器均已恢复。
        {
            void* pResult = queryText(operation);
            if (pResult == NULL || xrtGetError() != NULL) {
                xrtFree(pResult);
                return fail("allocation retry");
            }
            xrtFree(pResult);
        }
    }
    return 0;
}

static int testInputLimit(void)
{
    const size_t limit = 8u * 1024u * 1024u;
    char* text = (char*)xrtMalloc(limit + 2u);
    int result = 0;

    if (text == NULL) return fail("input limit fixture allocation");
    memset(text, ' ', limit + 1u);
    text[limit] = 0;
    text[limit + 1u] = 0;
    if (!xhcl_validate(text) || xrtGetError() != NULL) {
        result = fail("exact input limit must be accepted");
        goto done;
    }
    text[limit] = ' ';
    if (xhcl_validate(text) || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_RANGE) {
        result = fail("input above limit must report range error");
        goto done;
    }
    if (xhcl_get(text, "missing") != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_RANGE) {
        result = fail("query must use the same input limit");
        goto done;
    }
    text[limit + 1u] = ' ';
    if (xhcl_validate(text) || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_RANGE) {
        result = fail("unterminated overlimit input must stop at the budget");
        goto done;
    }
    text[limit] = 0;
    if (!xhcl_validate(text) || xrtGetError() != NULL) {
        result = fail("input limit failure must recover");
    }
done:
    xrtFree(text);
    return result;
}

// 截断或替换后，校验与查询必须对同一份文档作出一致判断。
static int testDocumentConsistency(cstr sHcl, bool bMustReject)
{
    bool bValid = xhcl_validate(sHcl);
    void* pResult;
    int64_t iCount;

    if ((bMustReject && bValid) || (xrtGetError() == NULL) != bValid)
        return fail("mutated document validation status");

    pResult = xhcl_get(sHcl, "name");
    if ((pResult != NULL) != bValid || (xrtGetError() == NULL) != bValid) {
        xrtFree(pResult);
        return fail("mutated document get status");
    }
    xrtFree(pResult);

    pResult = xhcl_get_raw(sHcl, "name");
    if ((pResult != NULL) != bValid || (xrtGetError() == NULL) != bValid) {
        xrtFree(pResult);
        return fail("mutated document getRaw status");
    }
    xrtFree(pResult);

    pResult = xhcl_block_label(sHcl, "resource", 0);
    if ((pResult != NULL) != bValid || (xrtGetError() == NULL) != bValid) {
        xrtFree(pResult);
        return fail("mutated document blockLabel status");
    }
    xrtFree(pResult);

    iCount = xhcl_count_blocks(sHcl, "resource");
    if ((iCount >= 0) != bValid || (xrtGetError() == NULL) != bValid)
        return fail("mutated document countBlocks status");
    return 0;
}

static int testDocumentMutations(void)
{
    static const char sFixture[] =
        "# lead\nname = \"a\\u4F60b\"\nresource \"web\" { tag = [1, 2] }\n";
    static const unsigned char Replacements[] = {'{', '}', '"', '\n', '#', 0xffu};
    char sChanged[sizeof(sFixture)];
    size_t i, j;

    memcpy(sChanged, sFixture, sizeof(sFixture));
    if (testDocumentConsistency(sChanged, false) != 0) return 1;
    for (i = 0; i < sizeof(sFixture) - 1u; ++i) {
        sChanged[i] = 0;
        if (testDocumentConsistency(sChanged, false) != 0) return 1;
        sChanged[i] = sFixture[i];
    }
    for (i = 0; i < sizeof(sFixture) - 1u; ++i) {
        for (j = 0; j < sizeof(Replacements); ++j) {
            if (sFixture[i] == (char)Replacements[j]) continue;
            sChanged[i] = (char)Replacements[j];
            if (testDocumentConsistency(sChanged, Replacements[j] == 0xffu) != 0)
                return 1;
        }
        sChanged[i] = sFixture[i];
    }
    return 0;
}

int main(void)
{
    cstr sHcl = "# comment\nresource \"aws_instance\" \"web\" {\n"
        "  name = \"web = primary\"\n  tags = {\n    role = \"api\"\n  }\n"
        "  nested { enabled = false }\n}\n";
    cstr sInvalidUtf8 = "name = \"\xff\"\n";
    cstr InvalidStrings[] = {
        "name = \"first\nsecond\"\n",
        "name = \"first\rsecond\"\n",
        "name = \"first\fsecond\"\n",
        "name = \"first\\\nsecond\"\n",
        "resource \"first\nsecond\" {}\n"
    };
    cstr InvalidEscapes[] = {
        "name = \"bad\\q\"\n",
        "name = \"bad\\u12\"\n",
        "name = \"bad\\uD800\"\n",
        "name = \"bad\\U00110000\"\n"
    };
    cstr InvalidDelimiters[] = {
        "name = ([)]\n",
        "name = ({)}\n"
    };
    void* pHeld = xhcl_get(sHcl, "name");
    char* pLarge;
    void* pLargeResult;
    int i;

    if (testInputLimit() != 0) return 1;

    for (i = 0; i < 12; ++i) {
        if (!ownedEquals(xhcl_get(sHcl, "name"), "web = primary")) {
            xrtFree(pHeld);
            return fail("repeated owned result");
        }
    }
    if (pHeld == NULL || strcmp((cstr)pHeld, "web = primary") != 0) {
        xrtFree(pHeld);
        return fail("held result changed after later queries");
    }
    xrtFree(pHeld);

    pLarge = (char*)xrtMalloc(12012u);
    if (pLarge == NULL) return fail("large fixture allocation");
    memcpy(pLarge, "value = \"", 9u);
    memset(pLarge + 9u, 'x', 12000u);
    memcpy(pLarge + 12009u, "\"\n", 3u);
    pLargeResult = xhcl_get(pLarge, "value");
    if (pLargeResult == NULL || strlen((cstr)pLargeResult) != 12000u ||
        ((cstr)pLargeResult)[11999] != 'x') {
        xrtFree(pLargeResult);
        xrtFree(pLarge);
        return fail("large owned result");
    }
    xrtFree(pLargeResult);
    xrtFree(pLarge);

    if (xhcl_has("name = \"unterminated", "name") ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("malformed query lost its value error");
    }
    xrtClearError();

    if (xhcl_has("name = 1\n", "") || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) {
        return fail("empty attribute name was treated as missing");
    }
    if (xhcl_has("name = 1\n", NULL) || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) {
        return fail("null attribute name was treated as missing");
    }
    if (!xhcl_has("name = 1\n", "name") || xrtGetError() != NULL) {
        return fail("valid attribute query did not clear the previous error");
    }

    if (xhcl_validate(sInvalidUtf8) || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("invalid UTF-8 text was accepted");
    }
    xrtClearError();
    if (xhcl_get(sInvalidUtf8, "name") != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("invalid UTF-8 query was accepted");
    }
    xrtClearError();
    if (!ownedEquals(xhcl_get("city = \"北京\"\n", "city"), "北京")) {
        return fail("valid UTF-8 query was rejected");
    }

    if (fabs(xhcl_get_float("ratio = 1.25\n", "ratio") - 1.25) > 0.000001) {
        return fail("decimal HCL float conversion failed");
    }
    if (xhcl_get_int("count = \" 42\"\n", "count") != 0 ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("HCL int accepted leading whitespace");
    }
    if (xhcl_get_int("count = -42\n", "count") != -42 || xrtGetError() != NULL) {
        return fail("valid HCL int did not recover after conversion error");
    }
    if (xhcl_get_float("ratio = \"0x1p2\"\n", "ratio") != 0.0 ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("non-decimal HCL float was accepted");
    }
    xrtClearError();
    {
        const char* oldLocale = setlocale(LC_NUMERIC, NULL);
        char* savedLocale = xrtStrDup(oldLocale != NULL ? oldLocale : "C");
        const char* candidates[] = {"German_Germany.1252", "de_DE.UTF-8", "fr_FR.UTF-8"};
        bool foundComma = false, validDecimal = true, rejectedComma = true;
        size_t k;
        if (savedLocale == NULL) return fail("locale fixture allocation");
        for (k = 0; k < sizeof(candidates) / sizeof(candidates[0]); ++k) {
            if (setlocale(LC_NUMERIC, candidates[k]) != NULL &&
                strcmp(localeconv()->decimal_point, ",") == 0) {
                foundComma = true;
                break;
            }
        }
        if (foundComma) {
            validDecimal = fabs(xhcl_get_float("ratio = 1.25\n", "ratio") - 1.25) < 0.000001;
            rejectedComma = xhcl_get_float("ratio = \"1,25\"\n", "ratio") == 0.0 &&
                xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE;
        }
        setlocale(LC_NUMERIC, savedLocale);
        xrtFree(savedLocale);
        if (!validDecimal || !rejectedComma) return fail("HCL float depends on process locale");
        xrtClearError();
    }

    for (i = 0; i < (int)(sizeof(InvalidStrings) / sizeof(InvalidStrings[0])); ++i) {
        if (xhcl_validate(InvalidStrings[i]) || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            return fail("quoted HCL string accepted a physical line break");
        }
        xrtClearError();
        if (xhcl_get(InvalidStrings[i], "name") != NULL ||
            xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            return fail("HCL query accepted a physical line break in a quoted string");
        }
        xrtClearError();
    }
    if (!xhcl_validate("name = \"first\\nsecond\"\n")) {
        return fail("escaped newline text was rejected");
    }
    for (i = 0; i < (int)(sizeof(InvalidDelimiters) / sizeof(InvalidDelimiters[0])); ++i) {
        if (xhcl_validate(InvalidDelimiters[i]) || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            return fail("crossed HCL expression delimiters were accepted");
        }
        xrtClearError();
        if (xhcl_get(InvalidDelimiters[i], "name") != NULL ||
            xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            return fail("HCL query accepted crossed expression delimiters");
        }
        xrtClearError();
    }
    if (!xhcl_validate("name = ([{}])\n")) {
        return fail("properly nested expression delimiters were rejected");
    }
    if (!xhcl_validate("name = \"([)]\"\n")) {
        return fail("quoted delimiters were parsed as expression syntax");
    }
    {
        char DeepExpression[160];
        size_t Pos = 0u;
        memcpy(DeepExpression, "name = ", 7u);
        Pos = 7u;
        for (i = 0; i < 64; ++i) DeepExpression[Pos++] = '(';
        DeepExpression[Pos++] = '1';
        for (i = 0; i < 64; ++i) DeepExpression[Pos++] = ')';
        DeepExpression[Pos++] = '\n';
        DeepExpression[Pos] = 0;
        if (!xhcl_validate(DeepExpression)) {
            return fail("64 nested HCL expression delimiters were rejected");
        }
        memcpy(DeepExpression, "name = ", 7u);
        Pos = 7u;
        for (i = 0; i < 65; ++i) DeepExpression[Pos++] = '(';
        DeepExpression[Pos++] = '1';
        for (i = 0; i < 65; ++i) DeepExpression[Pos++] = ')';
        DeepExpression[Pos++] = '\n';
        DeepExpression[Pos] = 0;
        if (xhcl_validate(DeepExpression) || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_RANGE) {
            return fail("HCL expression nesting exceeded its limit");
        }
        xrtClearError();
    }
    {
        const char* sBadTail = "name = \"ready\"\ninvalid = \"unterminated";
        const char* sBadBlock = "resource \"web\" { invalid = }";
        void* pResult = xhcl_get(sBadTail, "name");
        if (pResult != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            xrtFree(pResult);
            return fail("attribute query skipped malformed suffix");
        }
        xrtClearError();
        pResult = xhcl_block_label(sBadBlock, "resource", 0);
        if (pResult != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            xrtFree(pResult);
            return fail("block query skipped malformed body");
        }
        xrtClearError();
    }
    {
        char deep[512];
        size_t pos = 0u;
        for (i = 0; i < 64; ++i) {
            memcpy(deep + pos, "b {\n", 4u);
            pos += 4u;
        }
        for (i = 0; i < 64; ++i) {
            memcpy(deep + pos, "}\n", 2u);
            pos += 2u;
        }
        deep[pos] = 0;
        if (!xhcl_validate(deep)) return fail("64 nested HCL blocks were rejected");
        pos = 0u;
        for (i = 0; i < 65; ++i) {
            memcpy(deep + pos, "b {\n", 4u);
            pos += 4u;
        }
        for (i = 0; i < 65; ++i) {
            memcpy(deep + pos, "}\n", 2u);
            pos += 2u;
        }
        deep[pos] = 0;
        if (xhcl_validate(deep) || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_RANGE) {
            return fail("deeply nested HCL was not bounded");
        }
        xrtClearError();
    }
    for (i = 0; i < (int)(sizeof(InvalidEscapes) / sizeof(InvalidEscapes[0])); ++i) {
        if (xhcl_validate(InvalidEscapes[i]) || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            return fail("invalid HCL escape was accepted");
        }
        xrtClearError();
    }
    if (!xhcl_validate("name = \"\\u4F60\\U0001F642\"\n") ||
        !ownedEquals(xhcl_get("name = \"\\u4F60\\U0001F642\"\n", "name"), "你🙂")) {
        return fail("valid Unicode HCL escapes did not decode");
    }
    {
        cstr sCommented = "name/* token */ = /* lead */ \"web\" /* tail */\n"
            "count = 42/* number */\n"
            "resource/* type */\"srv\"/* brace */ { enabled = true/* bool */ }\n";
        if (!xhcl_validate(sCommented) ||
            !ownedEquals(xhcl_get(sCommented, "name"), "web") ||
            !ownedEquals(xhcl_get_raw(sCommented, "name"),
                "\"web\" /* tail */") ||
            xhcl_get_int(sCommented, "count") != 42 ||
            !xhcl_get_bool(sCommented, "enabled") ||
            xhcl_count_blocks(sCommented, "resource") != 1 ||
            !ownedEquals(xhcl_block_label(sCommented, "resource", 0), "srv")) {
            return fail("inline block comments changed HCL token meaning");
        }
        if (xhcl_validate("na/* split */me = 1\n") ||
            xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE)
            return fail("comment spliced an identifier");
        if (!ownedEquals(xhcl_get("name = \"a/*b*/c\"/* tail */\n", "name"),
                "a/*b*/c") ||
            !ownedEquals(xhcl_get("items = [1, /* middle */ 2] /* tail */\n", "items"),
                "[1, /* middle */ 2]") ||
            !ownedEquals(xhcl_get_raw("items = [1, /* middle */ 2] /* tail */\n", "items"),
                "[1, /* middle */ 2] /* tail */"))
            return fail("comment trimming consumed expression content");
    }
    if (!xhcl_validate("name = \"a\\u0000b\"\n") ||
        !ownedEquals(xhcl_get_raw("name = \"a\\u0000b\"\n", "name"),
            "\"a\\u0000b\"") ||
        xhcl_get("name = \"a\\u0000b\"\n", "name") != NULL ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
        return fail("decoded NUL was silently truncated");
    }
    xrtClearError();

    if (!xhcl_validate(sHcl) || xhcl_count_blocks(sHcl, "resource") != 1 ||
        xhcl_count_blocks(sHcl, "nested") != 1 ||
        xhcl_block_label_count(sHcl, "resource", 0) != 2 ||
        !ownedEquals(xhcl_block_label_at(sHcl, "resource", 0, 1), "web") ||
        !ownedEquals(xhcl_get(sHcl, "name"), "web = primary") ||
        !ownedEquals(xhcl_get_raw(sHcl, "tags"), "{\n    role = \"api\"\n  }") ||
        xhcl_count_attrs(sHcl) != 3 ||
        xhcl_validate("resource \"bad\" { name = \"unterminated }") ) {
        return fail("syntax-aware queries");
    }
    if (testAllocationFailures() != 0 || testDocumentMutations() != 0) return 1;
    puts("hcl C smoke: PASS");
    return 0;
}
