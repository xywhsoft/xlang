#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../gettext_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "gettext context test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static bool rejected(const char *result, xerrkind kind)
{
    const xerror *error = xrtGetError();
    return result == NULL && error != NULL && xrtErrorKind(error) == kind;
}

static bool text_is(const char *result, const char *expected)
{
    return result != NULL && strcmp(result, expected) == 0 && xrtGetError() == NULL;
}

int main(void)
{
    static const struct { const char *context, *id; } missing[] = {
        {NULL, "one"}, {"menu", NULL}, {NULL, NULL}
    };
    static const char singular[] = "one", plural[] = "many";
    xvalue *invalid[5];
    xvalue *catalog;
    char *owned;
    const char *borrowed;
    size_t i, j;
    bool failed;

    /* 缺上下文不能绕过目录类型验证，借用结果也不能伪装成原文回退。 */
    CHECK(rejected(xgt_tc_borrowed(NULL, NULL, singular), XERR_TYPE));
    invalid[0] = NULL;
    invalid[1] = xrtValueNull();
    invalid[2] = xrtValueInt(42);
    invalid[3] = xrtValueString(XRT_STR_LITERAL("not a catalog"));
    invalid[4] = xrtValueArray();
    CHECK(invalid[1] && invalid[2] && invalid[3] && invalid[4]);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        for (j = 0; j < sizeof(missing) / sizeof(missing[0]); ++j) {
            CHECK(rejected(xgt_tc_borrowed(invalid[i], missing[j].context,
                missing[j].id), XERR_TYPE));
            CHECK(rejected(xgt_tnc_borrowed(invalid[i], missing[j].context,
                missing[j].id, plural, 2), XERR_TYPE));
            owned = xgt_tc(invalid[i], missing[j].context, missing[j].id);
            failed = rejected(owned, XERR_TYPE);
            xrtFree(owned);
            CHECK(failed);
            owned = xgt_tnc(invalid[i], missing[j].context, missing[j].id, plural, 2);
            failed = rejected(owned, XERR_TYPE);
            xrtFree(owned);
            CHECK(failed);
        }
        xrtValueRelease(invalid[i]);
    }

    catalog = xgt_parse_po(
        "msgid \"\"\nmsgstr \"Plural-Forms: nplurals=2; plural=n==7?1/0:n!=1;\\n\"\n\n"
        "msgid \"one\"\nmsgstr \"plain\"\n\n"
        "msgctxt \"\"\nmsgid \"one\"\nmsgstr \"empty context\"\n\n"
        "msgctxt \"menu\"\nmsgid \"one\"\nmsgstr \"menu one\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT && !xrtGetError());
    CHECK(text_is(xgt_t_borrowed(catalog, singular), "plain"));
    CHECK(text_is(xgt_tc_borrowed(catalog, "", singular), "empty context"));
    CHECK(text_is(xgt_tc_borrowed(catalog, "menu", singular), "menu one"));

    /* NULL 与空上下文不同：缺参保留原文回退，结果仍区分借用和拥有。 */
    xrtSetErrorInfo(XERR_IO, "gettext.test", 0, "stale error");
    borrowed = xgt_tc_borrowed(catalog, NULL, singular);
    CHECK(borrowed == singular && xrtGetError() == NULL);
    owned = xgt_tc(catalog, NULL, singular);
    CHECK(owned && owned != singular && strcmp(owned, singular) == 0 && !xrtGetError());
    xrtFree(owned);
    CHECK(text_is(xgt_tc_borrowed(catalog, "menu", NULL), ""));
    CHECK(text_is(xgt_tc_borrowed(catalog, NULL, NULL), ""));
    CHECK(xgt_tnc_borrowed(catalog, NULL, singular, plural, 1) == singular);
    CHECK(xgt_tnc_borrowed(catalog, NULL, singular, plural, 2) == plural);
    CHECK(xgt_tnc_borrowed(catalog, "menu", NULL, plural, 2) == plural);
    CHECK(text_is(xgt_tnc_borrowed(catalog, NULL, NULL, NULL, 2), ""));

    /* 任一字段缺省时，仍须拒绝实际提供的非法分隔符与坏复数计数。 */
    CHECK(rejected(xgt_tc_borrowed(catalog, NULL, "bad\004id"), XERR_VALUE));
    CHECK(rejected(xgt_tc_borrowed(catalog, "bad\004context", NULL), XERR_VALUE));
    CHECK(rejected(xgt_tnc_borrowed(catalog, NULL, singular, "bad\004plural", 2), XERR_VALUE));
    CHECK(rejected(xgt_tnc_borrowed(catalog, NULL, NULL, "bad\004plural", 2), XERR_VALUE));
    CHECK(rejected(xgt_tnc_borrowed(catalog, "bad\004context", NULL, plural, 2), XERR_VALUE));
    for (j = 0; j < sizeof(missing) / sizeof(missing[0]); ++j)
        CHECK(rejected(xgt_tnc_borrowed(catalog, missing[j].context,
            missing[j].id, plural, -1), XERR_VALUE));
    CHECK(rejected(xgt_tnc_borrowed(catalog, NULL, singular, plural, 7), XERR_VALUE));
    CHECK(xgt_tnc_borrowed(catalog, NULL, singular, plural, 2) == plural && !xrtGetError());
    xrtValueRelease(catalog);
    puts("gettext C context queries: PASS");
    return 0;
}
