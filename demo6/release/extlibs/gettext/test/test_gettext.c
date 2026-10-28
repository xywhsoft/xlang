#define XRT_MODULE_ALL
#include "xrt.h"
#include "../gettext_native.h"
#include <stdio.h>
#include <string.h>

_Static_assert(_Generic(&xgt_t,
    char *(*)(xvalue *, const char *): 1, default: 0), "xgt_t must return owned text");
_Static_assert(_Generic(&xgt_tn,
    char *(*)(xvalue *, const char *, const char *, int64_t): 1, default: 0),
    "xgt_tn must return owned text");
_Static_assert(_Generic(&xgt_tc,
    char *(*)(xvalue *, const char *, const char *): 1, default: 0),
    "xgt_tc must return owned text");
_Static_assert(_Generic(&xgt_tnc,
    char *(*)(xvalue *, const char *, const char *, const char *, int64_t): 1, default: 0),
    "xgt_tnc must return owned text");

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "gettext test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int rejected_catalog_type(void *result)
{
    const xerror *error = xrtGetError();
    int rejected = result == NULL && error &&
        xrtErrorKind(error) == XERR_TYPE;
    xrtFree(result);
    return rejected;
}

static void write32(unsigned char *out, unsigned value, int big)
{
    int i;
    for (i = 0; i < 4; ++i) out[i] = (unsigned char)(value >> (8 * (big ? 3 - i : i)));
}

static xvalue *sample_mo(int big)
{
    static const char original[] = "one file\0many files";
    static const char translated[] = "eine Datei\0viele Dateien";
    const unsigned original_offset = 44;
    const unsigned translated_offset = original_offset + sizeof(original);
    unsigned char bytes[128] = {0};
    xvalue *raw;
    xvalue *catalog;

    write32(bytes, 0x950412DEu, big);
    write32(bytes + 8, 1, big);
    write32(bytes + 12, 28, big);
    write32(bytes + 16, 36, big);
    write32(bytes + 28, sizeof(original) - 1, big);
    write32(bytes + 32, original_offset, big);
    write32(bytes + 36, sizeof(translated) - 1, big);
    write32(bytes + 40, translated_offset, big);
    memcpy(bytes + original_offset, original, sizeof(original));
    memcpy(bytes + translated_offset, translated, sizeof(translated));
    raw = xrtValueBytes((xbytesview){ bytes, translated_offset + sizeof(translated) });
    if (!raw) return NULL;
    catalog = xgt_parse_mo(raw);
    xrtValueRelease(raw);
    return catalog;
}

static xvalue *sample_plural_mo(int big)
{
    static const char original[] = "file\0files";
    static const char translated[] = "one\0two\0many";
    static const char metadata[] =
        "Plural-Forms: nplurals=3; plural=n==1?0:n==2?1:2;\n";
    unsigned char bytes[256] = {0};
    unsigned original_offset = 60;
    unsigned translation_offset = original_offset + 1 + sizeof(original);
    unsigned plural_offset = translation_offset + sizeof(metadata);
    xvalue *raw, *catalog;

    write32(bytes, 0x950412DEu, big);
    write32(bytes + 8, 2, big);
    write32(bytes + 12, 28, big);
    write32(bytes + 16, 44, big);
    write32(bytes + 28, 0, big);
    write32(bytes + 32, original_offset, big);
    write32(bytes + 36, sizeof(original) - 1, big);
    write32(bytes + 40, original_offset + 1, big);
    write32(bytes + 44, sizeof(metadata) - 1, big);
    write32(bytes + 48, translation_offset, big);
    write32(bytes + 52, sizeof(translated) - 1, big);
    write32(bytes + 56, plural_offset, big);
    memcpy(bytes + original_offset + 1, original, sizeof(original));
    memcpy(bytes + translation_offset, metadata, sizeof(metadata));
    memcpy(bytes + plural_offset, translated, sizeof(translated));
    raw = xrtValueBytes((xbytesview){bytes, plural_offset + sizeof(translated)});
    if (!raw) return NULL;
    catalog = xgt_parse_mo(raw);
    xrtValueRelease(raw);
    return catalog;
}

static int test_mo_extra_original_part(void)
{
    static const char original[] = "one\0many\0extra";
    static const char translated[] = "uno\0much";
    unsigned char bytes[96] = {0};
    int endian;
    for (endian = 0; endian <= 1; ++endian) {
        unsigned translation_offset = 44u + sizeof(original);
        xvalue *raw, *catalog;
        write32(bytes, 0x950412DEu, endian);
        write32(bytes + 8, 1u, endian);
        write32(bytes + 12, 28u, endian);
        write32(bytes + 16, 36u, endian);
        write32(bytes + 28, sizeof(original) - 1u, endian);
        write32(bytes + 32, 44u, endian);
        write32(bytes + 36, sizeof(translated) - 1u, endian);
        write32(bytes + 40, translation_offset, endian);
        memcpy(bytes + 44, original, sizeof(original));
        memcpy(bytes + translation_offset, translated, sizeof(translated));
        raw = xrtValueBytes((xbytesview){ bytes,
            translation_offset + sizeof(translated) });
        if (!raw) return 0;
        catalog = xgt_parse_mo(raw);
        xrtValueRelease(raw);
        if (!catalog || xrtValueType(catalog) != XVALUE_NULL ||
            !xrtGetError() || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            xrtValueRelease(catalog);
            return 0;
        }
        xrtValueRelease(catalog);
    }
    return 1;
}

static int test_mo_context_separator(void)
{
    static const char valid[] = "menu\004Open";
    static const char invalid[] = "menu\004sub\004Open";
    const char *originals[] = { valid, invalid };
    unsigned char bytes[96] = {0};
    int endian, variant;
    for (endian = 0; endian <= 1; ++endian) {
        for (variant = 0; variant < 2; ++variant) {
            const char *original = originals[variant];
            size_t length = strlen(original);
            xvalue *raw, *catalog;
            char *translated;
            memset(bytes, 0, sizeof(bytes));
            write32(bytes, 0x950412DEu, endian);
            write32(bytes + 8, 1u, endian);
            write32(bytes + 12, 28u, endian);
            write32(bytes + 16, 36u, endian);
            write32(bytes + 28, (unsigned)length, endian);
            write32(bytes + 32, 44u, endian);
            write32(bytes + 36, 5u, endian);
            write32(bytes + 40, 44u + (unsigned)length + 1u, endian);
            memcpy(bytes + 44, original, length);
            memcpy(bytes + 45 + length, "shown", 6u);
            raw = xrtValueBytes((xbytesview){ bytes, 51u + length });
            if (!raw) return 0;
            catalog = xgt_parse_mo(raw);
            xrtValueRelease(raw);
            if (!catalog) return 0;
            if (variant == 0) {
                if (xrtValueType(catalog) != XVALUE_OBJECT) {
                    xrtValueRelease(catalog);
                    return 0;
                }
                translated = xgt_tc(catalog, "menu", "Open");
                if (!translated || strcmp(translated, "shown") != 0) {
                    xrtFree(translated);
                    xrtValueRelease(catalog);
                    return 0;
                }
                xrtFree(translated);
                translated = xgt_t(catalog, "menu\004Open");
                if (translated || !xrtGetError() ||
                    xrtErrorKind(xrtGetError()) != XERR_VALUE) {
                    xrtFree(translated);
                    xrtValueRelease(catalog);
                    return 0;
                }
                translated = xgt_tc(catalog, "menu\004sub", "Open");
                if (translated || !xrtGetError() ||
                    xrtErrorKind(xrtGetError()) != XERR_VALUE) {
                    xrtFree(translated);
                    xrtValueRelease(catalog);
                    return 0;
                }
            } else if (xrtValueType(catalog) != XVALUE_NULL ||
                !xrtGetError() || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
                xrtValueRelease(catalog);
                return 0;
            }
            xrtValueRelease(catalog);
        }
    }
    return 1;
}

/* MO 长度不包含结束 NUL；原文和译文的终止字节都必须存在。 */
static int test_mo_strings(void)
{
    unsigned char bytes[56] = {0};
    int endian, target;
    for (endian = 0; endian <= 1; ++endian) {
        write32(bytes, 0x950412DEu, endian);
        write32(bytes + 8, 1, endian);
        write32(bytes + 12, 28, endian);
        write32(bytes + 16, 36, endian);
        write32(bytes + 28, 5, endian);
        write32(bytes + 32, 44, endian);
        write32(bytes + 36, 5, endian);
        write32(bytes + 40, 50, endian);
        memcpy(bytes + 44, "hello", 5);
        memcpy(bytes + 50, "world", 5);
        {
            xvalue *raw = xrtValueBytes((xbytesview){bytes, sizeof(bytes)});
            xvalue *catalog;
            char *translated;
            if (!raw) return 0;
            catalog = xgt_parse_mo(raw);
            xrtValueRelease(raw);
            if (!catalog || xrtValueType(catalog) != XVALUE_OBJECT) {
                xrtValueRelease(catalog);
                return 0;
            }
            translated = xgt_t(catalog, "hello");
            if (!translated || strcmp(translated, "world") != 0) {
                xrtFree(translated);
                xrtValueRelease(catalog);
                return 0;
            }
            xrtFree(translated);
            xrtValueRelease(catalog);
        }
        for (target = 0; target < 2; ++target) {
            xvalue *raw, *catalog;
            size_t terminator = target ? 55u : 49u;
            bytes[terminator] = 'x';
            raw = xrtValueBytes((xbytesview){bytes, sizeof(bytes)});
            if (!raw) return 0;
            catalog = xgt_parse_mo(raw);
            xrtValueRelease(raw);
            if (!catalog || xrtValueType(catalog) != XVALUE_NULL ||
                !xrtGetError() || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
                xrtValueRelease(catalog);
                return 0;
            }
            xrtValueRelease(catalog);
            bytes[terminator] = 0;
        }
        for (target = 0; target < 2; ++target) {
            xvalue *raw, *catalog;
            size_t position = target ? 50u : 44u;
            unsigned char original = bytes[position];
            bytes[position] = 0xffu;
            raw = xrtValueBytes((xbytesview){bytes, sizeof(bytes)});
            if (!raw) return 0;
            catalog = xgt_parse_mo(raw);
            xrtValueRelease(raw);
            if (!catalog || xrtValueType(catalog) != XVALUE_NULL ||
                !xrtGetError() || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
                xrtValueRelease(catalog);
                return 0;
            }
            xrtValueRelease(catalog);
            bytes[position] = original;
        }
        {
            xvalue *raw, *catalog;
            bytes[52] = 0;
            raw = xrtValueBytes((xbytesview){bytes, sizeof(bytes)});
            if (!raw) return 0;
            catalog = xgt_parse_mo(raw);
            xrtValueRelease(raw);
            if (!catalog || xrtValueType(catalog) != XVALUE_NULL ||
                !xrtGetError() || xrtErrorKind(xrtGetError()) != XERR_VALUE) {
                xrtValueRelease(catalog);
                return 0;
            }
            xrtValueRelease(catalog);
            bytes[52] = 'r';
        }
    }
    return 1;
}

static int test_po_allocations(void)
{
    static const char source[] =
        "msgid \"\"\nmsgstr \"Plural-Forms: nplurals=2; plural=n!=1;\\n\"\n\n"
        "msgctxt \"menu\"\nmsgid \"Open\"\nmsgstr \"Ouvrir\"\n\n"
        "msgid \"file\"\nmsgid_plural \"files\"\n"
        "msgstr[0] \"fichier\"\nmsgstr[1] \"fichiers\"\n";
    unsigned point;
    unsigned failures = 0;
    int completed = 0;
    for (point = 0; point < 128; ++point) {
        xvalue *catalog;
        int triggered;
        const xerror *error;
        if (!xrtMemDebugFailAfter(point)) return 0;
        catalog = xgt_parse_po(source);
        triggered = xrtMemDebugFailTriggered();
        error = xrtGetError();
        xrtMemDebugFailClear();
        if (triggered) {
            failures++;
            if ((catalog && xrtValueType(catalog) != XVALUE_NULL) ||
                !error || xrtErrorKind(error) != XERR_MEMORY) {
                fprintf(stderr, "PO allocation failure %u was not MEMORY\n", point);
                xrtValueRelease(catalog);
                return 0;
            }
        } else if (catalog && xrtValueType(catalog) == XVALUE_OBJECT && !error) {
            completed = 1;
        }
        xrtValueRelease(catalog);
        if (completed) break;
    }
    return completed && failures >= 8;
}

static int test_mo_allocations(void)
{
    static const char original[] = "one file\0many files";
    static const char translated[] = "eine Datei\0viele Dateien";
    const unsigned original_offset = 44;
    const unsigned translated_offset = original_offset + sizeof(original);
    unsigned char bytes[128] = {0};
    xvalue *raw;
    unsigned point;
    unsigned failures = 0;
    int completed = 0;
    write32(bytes, 0x950412DEu, 0);
    write32(bytes + 8, 1, 0);
    write32(bytes + 12, 28, 0);
    write32(bytes + 16, 36, 0);
    write32(bytes + 28, sizeof(original) - 1, 0);
    write32(bytes + 32, original_offset, 0);
    write32(bytes + 36, sizeof(translated) - 1, 0);
    write32(bytes + 40, translated_offset, 0);
    memcpy(bytes + original_offset, original, sizeof(original));
    memcpy(bytes + translated_offset, translated, sizeof(translated));
    raw = xrtValueBytes((xbytesview){
        bytes, translated_offset + sizeof(translated) });
    if (!raw) return 0;
    for (point = 0; point < 64; ++point) {
        xvalue *catalog;
        int triggered;
        const xerror *error;
        if (!xrtMemDebugFailAfter(point)) break;
        catalog = xgt_parse_mo(raw);
        triggered = xrtMemDebugFailTriggered();
        error = xrtGetError();
        xrtMemDebugFailClear();
        if (triggered) {
            failures++;
            if ((catalog && xrtValueType(catalog) != XVALUE_NULL) ||
                !error || xrtErrorKind(error) != XERR_MEMORY) {
                fprintf(stderr, "MO allocation failure %u was not MEMORY\n", point);
                xrtValueRelease(catalog);
                break;
            }
        } else if (catalog && xrtValueType(catalog) == XVALUE_OBJECT && !error) {
            completed = 1;
        }
        xrtValueRelease(catalog);
        if (completed) break;
    }
    xrtValueRelease(raw);
    return completed && failures >= 3;
}

static int test_six_form_plural(void)
{
    static const char po[] =
        "msgid \"\"\n"
        "msgstr \"Plural-Forms: nplurals=6; plural=n==0?0:n==1?1:n==2?2:(n%100>=3&&n%100<=10)?3:(n%100>=11&&n%100<=99)?4:5;\\n\"\n\n"
        "msgid \"item\"\nmsgid_plural \"items\"\n"
        "msgstr[0] \"zero\"\nmsgstr[1] \"one\"\n"
        "msgstr[2] \"two\"\nmsgstr[3] \"few\"\n"
        "msgstr[4] \"many\"\nmsgstr[5] \"other\"\n\n"
        "msgctxt \"counter\"\nmsgid \"item\"\nmsgid_plural \"items\"\n"
        "msgstr[0] \"ctx zero\"\nmsgstr[1] \"ctx one\"\n"
        "msgstr[2] \"ctx two\"\nmsgstr[3] \"ctx few\"\n"
        "msgstr[4] \"ctx many\"\nmsgstr[5] \"ctx other\"\n";
    static const struct { int64_t count; const char *expected; } cases[] = {
        {0, "zero"}, {1, "one"}, {2, "two"}, {3, "few"},
        {10, "few"}, {11, "many"}, {99, "many"}, {100, "other"},
        {103, "few"}, {111, "many"}, {200, "other"}
    };
    xvalue *catalog = xgt_parse_po(po);
    size_t i;
    int ok = catalog && xrtValueType(catalog) == XVALUE_OBJECT;
    if (ok) {
        /* 多个条件和取模同时参与复数选择，验证每个结果槽。 */
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char *result = xgt_tn(catalog, "item", "items", cases[i].count);
            if (!result || strcmp(result, cases[i].expected) != 0) ok = 0;
            xrtFree(result);
        }
        /* GNU 式无计数查询只取首形式，不能假定第 0 槽就是 n=1。 */
        for (i = 0; i < 2; ++i) {
            const char *expected = i ? "ctx zero" : "zero";
            const char *borrowed = i ? xgt_tc_borrowed(catalog, "counter", "item") :
                xgt_t_borrowed(catalog, "item");
            char *owned = i ? xgt_tc(catalog, "counter", "item") : xgt_t(catalog, "item");
            if (!borrowed || !owned || strcmp(borrowed, expected) != 0 ||
                strcmp(owned, expected) != 0 || xrtGetError()) ok = 0;
            xrtFree(owned);
        }
        {
            char *one = xgt_tnc(catalog, "counter", "item", "items", 1);
            if (!one || strcmp(one, "ctx one") != 0 || xrtGetError()) ok = 0;
            xrtFree(one);
        }
    }
    xrtValueRelease(catalog);
    return ok;
}

static int test_plural_unary_depth(void)
{
    char formula[50], po[160];
    xvalue *catalog;
    int depth;
    for (depth = 47; depth <= 48; ++depth) {
        memset(formula, '!', (size_t)depth);
        formula[depth] = 'n';
        formula[depth + 1] = '\0';
        snprintf(po, sizeof(po),
            "msgid \"\"\nmsgstr \"Plural-Forms: nplurals=2; plural=%s;\\n\"\n",
            formula);
        catalog = xgt_parse_po(po);
        if (catalog == NULL) return 0;
        /* 一元链也应服从与括号/表达式相同的 48 层递归上限。 */
        if ((depth == 47 && xrtValueType(catalog) != XVALUE_OBJECT) ||
            (depth == 48 && xrtValueType(catalog) != XVALUE_NULL)) {
            xrtValueRelease(catalog);
            return 0;
        }
        xrtValueRelease(catalog);
    }
    return 1;
}

/* 保留有效魔数，扫描短文件和表项单比特损坏的读取边界。 */
static int test_mo_mutations(void)
{
    unsigned char bytes[56] = {0};
    size_t size, offset;
    int bit;
    write32(bytes, 0x950412DEu, 0);
    write32(bytes + 8, 1, 0);
    write32(bytes + 12, 28, 0);
    write32(bytes + 16, 36, 0);
    write32(bytes + 28, 5, 0);
    write32(bytes + 32, 44, 0);
    write32(bytes + 36, 5, 0);
    write32(bytes + 40, 50, 0);
    memcpy(bytes + 44, "hello", 5);
    memcpy(bytes + 50, "world", 5);
    for (size = 0; size <= sizeof(bytes); ++size) {
        xvalue *raw = xrtValueBytes((xbytesview){bytes, size});
        xvalue *catalog;
        int valid;
        if (!raw) return 0;
        catalog = xgt_parse_mo(raw);
        valid = !catalog || xrtValueType(catalog) == XVALUE_NULL ||
            xrtValueType(catalog) == XVALUE_OBJECT;
        xrtValueRelease(catalog);
        xrtValueRelease(raw);
        if (!valid) return 0;
    }
    for (offset = 4; offset < sizeof(bytes); ++offset) {
        for (bit = 0; bit < 8; ++bit) {
            xvalue *raw, *catalog;
            int valid;
            bytes[offset] ^= (unsigned char)(1u << bit);
            raw = xrtValueBytes((xbytesview){bytes, sizeof(bytes)});
            if (!raw) return 0;
            catalog = xgt_parse_mo(raw);
            valid = !catalog || xrtValueType(catalog) == XVALUE_NULL ||
                xrtValueType(catalog) == XVALUE_OBJECT;
            xrtValueRelease(catalog);
            xrtValueRelease(raw);
            bytes[offset] ^= (unsigned char)(1u << bit);
            if (!valid) return 0;
        }
    }
    return 1;
}

/* 从有效 PO 出发，逐个截断或翻转单比特，检查解析和释放边界。 */
static int test_po_mutations(void)
{
    char bytes[] =
        "msgid \"\"\n"
        "msgstr \"Plural-Forms: nplurals=2; plural=n!=1;\\n\"\n\n"
        "msgctxt \"menu\"\n"
        "msgid \"file\"\n"
        "msgid_plural \"files\"\n"
        "msgstr[0] \"one\"\n"
        "msgstr[1] \"many\"\n";
    size_t offset;
    int bit;
    xvalue *catalog = xgt_parse_po(bytes);
    if (!catalog || xrtValueType(catalog) != XVALUE_OBJECT) {
        xrtValueRelease(catalog);
        return 0;
    }
    xrtValueRelease(catalog);
    for (offset = 0; offset < sizeof(bytes); ++offset) {
        char saved = bytes[offset];
        bytes[offset] = 0;
        catalog = xgt_parse_po(bytes);
        if (!catalog || (xrtValueType(catalog) != XVALUE_OBJECT &&
                         xrtValueType(catalog) != XVALUE_NULL)) {
            xrtValueRelease(catalog);
            return 0;
        }
        xrtValueRelease(catalog);
        bytes[offset] = saved;
    }
    for (offset = 0; offset < sizeof(bytes) - 1; ++offset) {
        for (bit = 0; bit < 8; ++bit) {
            bytes[offset] ^= (char)(1u << bit);
            catalog = xgt_parse_po(bytes);
            if (!catalog || (xrtValueType(catalog) != XVALUE_OBJECT &&
                             xrtValueType(catalog) != XVALUE_NULL)) {
                xrtValueRelease(catalog);
                return 0;
            }
            xrtValueRelease(catalog);
            bytes[offset] ^= (char)(1u << bit);
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    char po[2600];
    char *result;
    xvalue *catalog;
    int endian;
    static const char localized[] =
        "msgid \"\"\n"
        "msgstr \"Plural-Forms: nplurals=3; plural=n==1?0:n==2?1:2;\\n\"\n\n"
        "msgctxt \"menu\"\nmsgid \"Open\"\nmsgstr \"menu open\"\n\n"
        "msgctxt \"verb\"\nmsgid \"Open\"\nmsgstr \"verb open\"\n\n"
        "msgctxt \"storage\"\nmsgid \"file\"\nmsgid_plural \"files\"\n"
        "msgstr[0] \"one\"\nmsgstr[1] \"two\"\nmsgstr[2] \"many\"\n";

    CHECK(argc == 2 && argv[1][0] != '\0');
    CHECK(test_po_allocations());
    CHECK(test_mo_allocations());
    CHECK(test_mo_strings());
    CHECK(test_mo_extra_original_part());
    CHECK(test_mo_context_separator());
    CHECK(test_mo_mutations());
    CHECK(test_po_mutations());
    CHECK(test_six_form_plural());
    CHECK(test_plural_unary_depth());
    catalog = xrtValueInt(42);
    CHECK(catalog != NULL);
    CHECK(rejected_catalog_type(xgt_t(catalog, "missing")));
    CHECK(rejected_catalog_type(xgt_tn(catalog, "one", "many", 2)));
    CHECK(rejected_catalog_type(xgt_tc(catalog, "menu", "missing")));
    CHECK(rejected_catalog_type(xgt_tnc(catalog, "menu", "one", "many", 2)));
    CHECK(rejected_catalog_type(xgt_t(NULL, "missing")));
    xrtValueRelease(catalog);
    catalog = xgt_parse_po("msgid \"x\"\nmsgstr \"\\377\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
    CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(catalog);
    catalog = xgt_parse_po("msgid \"x\"\nmsgstr \"\xff\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
    CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(catalog);

    memcpy(po, "msgid \"long\"\nmsgstr \"", 21);
    memset(po + 21, 'x', 2500);
    memcpy(po + 2521, "\"\n", 3);
    catalog = xgt_parse_po(po);
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    result = xgt_t(catalog, "long");
    CHECK(result && strlen(result) == 2500 && result[2499] == 'x');
    xrtFree(result);
    xrtValueRelease(catalog);

    catalog = xgt_parse_po(localized);
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    {
        xvalue *entry = xrtValueObjectGet(catalog, (xstrview){ "menu\004Open", 9 });
        xstrview translation;
        char missing[] = "not translated";
        CHECK(entry && xrtValueGetString(entry, &translation));
        CHECK(xgt_tc_borrowed(catalog, "menu", "Open") == translation.Data);
        CHECK(xgt_t_borrowed(catalog, missing) == missing);
        CHECK(xgt_tn_borrowed(catalog, missing, "plural", 2) != NULL);
        CHECK(strcmp(xgt_tnc_borrowed(catalog, "storage", "file", "files", 2), "two") == 0);
        CHECK(xgt_t_borrowed(NULL, missing) == NULL);
        CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_TYPE);
    }
    result = xgt_tc(catalog, "menu", "Open");
    CHECK(result && strcmp(result, "menu open") == 0);
    xrtFree(result);
    result = xgt_tc(catalog, "verb", "Open");
    CHECK(result && strcmp(result, "verb open") == 0);
    xrtFree(result);
    result = xgt_tnc(catalog, "storage", "file", "files", 1);
    CHECK(result && strcmp(result, "one") == 0);
    xrtFree(result);
    result = xgt_tnc(catalog, "storage", "file", "files", 2);
    CHECK(result && strcmp(result, "two") == 0);
    xrtFree(result);
    result = xgt_tnc(catalog, "storage", "file", "files", 9);
    CHECK(result && strcmp(result, "many") == 0);
    xrtFree(result);
    result = xgt_tc(catalog, "missing", "Open");
    CHECK(result && strcmp(result, "Open") == 0);
    xrtFree(result);
    CHECK(xrtMemDebugFailAfter(0));
    result = xgt_tc(catalog, "menu", "Open");
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(result == NULL && xrtGetError());
    xrtClearError();
    CHECK(xrtMemDebugFailAfter(0));
    result = xgt_tnc(catalog, "storage", "file", "files", 2);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(result == NULL && xrtGetError());
    xrtClearError();
    xrtValueRelease(catalog);

    catalog = xgt_parse_po(
        "#, fuzzy\nmsgid \"Review\"\nmsgstr \"unapproved\"\n\n"
        "msgid \"Blank\"\nmsgstr \"\"\n\n"
        "msgid \"Ready\"\nmsgstr \"done\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    result = xgt_t(catalog, "Review");
    CHECK(result && strcmp(result, "Review") == 0);
    xrtFree(result);
    result = xgt_t(catalog, "Blank");
    CHECK(result && strcmp(result, "Blank") == 0);
    xrtFree(result);
    result = xgt_t(catalog, "Ready");
    CHECK(result && strcmp(result, "done") == 0);
    xrtFree(result);
    xrtValueRelease(catalog);

    catalog = xgt_parse_po(
        "#, fuzzy\nmsgid \"\"\n"
        "msgstr \"Plural-Forms: nplurals=3; plural=n==1?0:n==2?1:2;\\n\"\n\n"
        "msgid \"item\"\nmsgid_plural \"items\"\n"
        "msgstr[0] \"one\"\nmsgstr[1] \"\"\nmsgstr[2] \"many\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    result = xgt_tn(catalog, "item", "items", 2);
    CHECK(result && strcmp(result, "items") == 0);
    xrtFree(result);
    result = xgt_tn(catalog, "item", "items", 9);
    CHECK(result && strcmp(result, "many") == 0);
    xrtFree(result);
    xrtValueRelease(catalog);

    catalog = xgt_parse_po("msgid \"x\"\nmsgstr \"unterminated\n");
    CHECK(!catalog || xrtValueType(catalog) == XVALUE_NULL);
    catalog = xgt_parse_po("msgid \"x\"\nmsgid_plural \"xs\"\nmsgstr[1] \"bad\"\n");
    CHECK(!catalog || xrtValueType(catalog) == XVALUE_NULL);
    catalog = xgt_parse_po(
        "msgid \"same\"\nmsgstr \"first\"\n\n"
        "msgid \"same\"\nmsgstr \"second\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
    CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(catalog);
    catalog = xgt_parse_po(
        "msgctxt \"menu\"\nmsgid \"same\"\nmsgstr \"menu\"\n\n"
        "msgctxt \"action\"\nmsgid \"same\"\nmsgstr \"action\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    xrtValueRelease(catalog);
    /* 0x04 是上下文键分隔符，不能由普通消息 ID 伪造。 */
    catalog = xgt_parse_po("msgid \"menu\\004Open\"\nmsgstr \"wrong\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
    CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(catalog);
    catalog = xgt_parse_po("msgctxt \"menu\\004sub\"\nmsgid \"Open\"\nmsgstr \"wrong\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
    CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(catalog);
    catalog = xgt_parse_po("msgid \"file\"\nmsgid_plural \"files\\004other\"\n"
        "msgstr[0] \"one\"\nmsgstr[1] \"many\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
    CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(catalog);
    catalog = xgt_parse_po("msgid \"\"\nmsgstr \"Plural-Forms: nplurals=2; plural=n/0;\\n\"\n");
    CHECK(!catalog || xrtValueType(catalog) == XVALUE_NULL);
    /* 导入时的有限抽样无法证明公式对所有计数都安全。 */
    catalog = xgt_parse_po(
        "msgid \"\"\nmsgstr \"Plural-Forms: nplurals=2; plural=n==7?1/0:n!=1;\\n\"\n\n"
        "msgid \"item\"\nmsgid_plural \"items\"\n"
        "msgstr[0] \"one\"\nmsgstr[1] \"many\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    result = xgt_tn(catalog, "item", "items", 8);
    CHECK(result && strcmp(result, "many") == 0 && xrtGetError() == NULL);
    xrtFree(result);
    result = xgt_tn(catalog, "item", "items", 7);
    CHECK(result == NULL && xrtGetError() &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE);
    result = xgt_tn(catalog, "item", "items", 1);
    CHECK(result && strcmp(result, "one") == 0 && xrtGetError() == NULL);
    xrtFree(result);
    xrtValueRelease(catalog);
    catalog = xgt_parse_po("msgid \"tab\"\nmsgstr \"a\\011b\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    result = xgt_t(catalog, "tab");
    CHECK(result && strcmp(result, "a\tb") == 0);
    xrtFree(result);
    xrtValueRelease(catalog);

    catalog = xgt_parse_po("msgid \"escaped\"\nmsgstr \"\\x41\\x42\\?\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    result = xgt_t(catalog, "escaped");
    CHECK(result && strcmp(result, "AB?") == 0);
    xrtFree(result);
    xrtValueRelease(catalog);
    catalog = xgt_parse_po("msgid \"utf8\"\nmsgstr \"\\xC3\\xA9\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    result = xgt_t(catalog, "utf8");
    CHECK(result && strcmp(result, "\xc3\xa9") == 0);
    xrtFree(result);
    xrtValueRelease(catalog);
    {
        static const char *invalid[] = {
            "msgid \"bad\"\nmsgstr \"\\x\"\n",
            "msgid \"bad\"\nmsgstr \"\\x100\"\n",
            "msgid \"bad\"\nmsgstr \"\\401\"\n",
            "msgid \"bad\"\nmsgstr \"\\xFF\"\n"
        };
        size_t i;
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            catalog = xgt_parse_po(invalid[i]);
            CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
            CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
            xrtValueRelease(catalog);
        }
    }

    /* 失败后同线程成功解析/查询，不应继续暴露旧错误。 */
    catalog = xgt_parse_po(NULL);
    CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(catalog);
    catalog = xgt_parse_po("msgid \"one\"\nmsgstr \"uno\"\n");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    CHECK(xrtGetError() == NULL);
    result = xgt_tn(catalog, "one", "many", -1);
    CHECK(result == NULL && xrtGetError());
    result = xgt_t(catalog, "one");
    CHECK(result && strcmp(result, "uno") == 0 && xrtGetError() == NULL);
    xrtFree(result);
    xrtValueRelease(catalog);

    catalog = xgt_parse_mo(NULL);
    CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(catalog);
    catalog = sample_mo(0);
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    CHECK(xrtGetError() == NULL);
    xrtValueRelease(catalog);

    for (endian = 0; endian <= 1; ++endian) {
        catalog = sample_mo(endian);
        CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
        result = xgt_t(catalog, "one file");
        CHECK(result && strcmp(result, "eine Datei") == 0);
        xrtFree(result);
        result = xgt_tn(catalog, "one file", "many files", 2);
        CHECK(result && strcmp(result, "viele Dateien") == 0);
        xrtFree(result);
        xrtValueRelease(catalog);

        catalog = sample_plural_mo(endian);
        CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
        result = xgt_tn(catalog, "file", "files", 2);
        CHECK(result && strcmp(result, "two") == 0);
        xrtFree(result);
        result = xgt_tn(catalog, "file", "files", 9);
        CHECK(result && strcmp(result, "many") == 0);
        xrtFree(result);
        xrtValueRelease(catalog);
    }
    for (endian = 0; endian <= 1; ++endian) {
        unsigned char bytes[68] = {0};
        xvalue *raw;
        write32(bytes, 0x950412DEu, endian);
        write32(bytes + 8, 2, endian);
        write32(bytes + 12, 28, endian);
        write32(bytes + 16, 44, endian);
        write32(bytes + 28, 3, endian);
        write32(bytes + 32, 60, endian);
        write32(bytes + 36, 3, endian);
        write32(bytes + 40, 60, endian);
        write32(bytes + 44, 1, endian);
        write32(bytes + 48, 64, endian);
        write32(bytes + 52, 1, endian);
        write32(bytes + 56, 66, endian);
        memcpy(bytes + 60, "key", 3);
        bytes[64] = 'A';
        bytes[66] = 'B';
        raw = xrtValueBytes((xbytesview){bytes, sizeof(bytes)});
        CHECK(raw != NULL);
        catalog = xgt_parse_mo(raw);
        CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
        CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(catalog);
        xrtValueRelease(raw);
    }
    {
        unsigned char bytes[50] = {0};
        xvalue *raw;
        write32(bytes, 0x950412DEu, 0);
        write32(bytes + 8, 1, 0);
        write32(bytes + 12, 28, 0);
        write32(bytes + 16, 36, 0);
        write32(bytes + 28, 5, 0);
        write32(bytes + 32, 44, 0);
        write32(bytes + 36, 0, 0);
        write32(bytes + 40, 49, 0);
        memcpy(bytes + 44, "Blank", 5);
        raw = xrtValueBytes((xbytesview){bytes, sizeof(bytes)});
        CHECK(raw != NULL);
        catalog = xgt_parse_mo(raw);
        CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
        result = xgt_t(catalog, "Blank");
        CHECK(result && strcmp(result, "Blank") == 0);
        xrtFree(result);
        xrtValueRelease(catalog);
        xrtValueRelease(raw);
    }
    {
        unsigned char invalid[28] = {0};
        xvalue *raw;
        write32(invalid, 0x950412DEu, 0);
        write32(invalid + 8, 1, 0);
        write32(invalid + 12, 1000, 0);
        raw = xrtValueBytes((xbytesview){invalid, sizeof(invalid)});
        CHECK(raw != NULL);
        catalog = xgt_parse_mo(raw);
        CHECK(!catalog || xrtValueType(catalog) == XVALUE_NULL);
        xrtValueRelease(raw);
    }
    /* 缺失文件属于文件系统错误，不能被包层改写成格式错误。 */
    {
        str missing = xrtPathJoin(argv[1], "never-created/missing.mo");
        CHECK(missing != NULL);
        CHECK(!xrtPathExists(missing));
        catalog = xgt_load_mo(missing);
        CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
        CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
        xrtValueRelease(catalog);
        xrtFree(missing);
    }
    catalog = xgt_load_mo("release/extlibs/gettext/test/invalid.mo");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_NULL);
    CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(catalog);
    {
        str path = xrtPathJoin(argv[1], "over_limit.mo");
        int rejected;
        CHECK(path != NULL);
        CHECK(xrtFileWriteAll(path, (xbytesview){NULL, 0u}));
        CHECK(xrtFileSetSize(path, 64u * 1024u * 1024u + 1u));
        catalog = xgt_load_mo(path);
        rejected = catalog && xrtValueType(catalog) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE;
        xrtValueRelease(catalog);
        CHECK(xrtFileDelete(path));
        xrtFree(path);
        CHECK(rejected);
    }
    {
        size_t size = 64u * 1024u * 1024u + 1u;
        char *text = xrtMalloc(size + 1u);
        int rejected;
        CHECK(text != NULL);
        memset(text, '#', size);
        text[size - 1u] = '\0';
        catalog = xgt_parse_po(text);
        CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT &&
            xrtGetError() == NULL);
        xrtValueRelease(catalog);
        text[size - 1u] = '#';
        text[size] = '\0';
        catalog = xgt_parse_po(text);
        rejected = catalog && xrtValueType(catalog) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE;
        xrtValueRelease(catalog);
        CHECK(rejected);
        text[size] = '#';
        catalog = xgt_parse_po(text);
        rejected = catalog && xrtValueType(catalog) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE;
        xrtValueRelease(catalog);
        xrtFree(text);
        CHECK(rejected);
    }
    {
        size_t size = 64u * 1024u * 1024u + 1u;
        bytes data = xrtMalloc(size);
        xvalue *raw;
        int rejected;
        CHECK(data != NULL);
        memset(data, 0, size);
        raw = xrtValueBytesTake(&data, size);
        CHECK(raw != NULL);
        catalog = xgt_parse_mo(raw);
        rejected = catalog && xrtValueType(catalog) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE;
        xrtValueRelease(catalog);
        xrtValueRelease(raw);
        CHECK(rejected);
    }
    catalog = xgt_load_mo("test/fixtures/gettext_mini.mo");
    CHECK(catalog && xrtValueType(catalog) == XVALUE_OBJECT);
    CHECK(xrtGetError() == NULL);
    result = xgt_t(catalog, "hello");
    CHECK(result && strcmp(result, "world!") == 0);
    xrtFree(result);
    xrtValueRelease(catalog);
    return 0;
}
