/* 凭据字段与宏正文共用资源预算；解析/写出按实际处理的节点计数。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../xrt_netrc_internal.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;

static void check(bool ok, const char* name, unsigned limit, int code)
{
    ++checks;
    if (!ok) {
        ++failures;
        fprintf(stderr, "%s limit=%u code=%d\n", name, limit, code);
    }
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes;
}

static void node_case(const char* text, unsigned nodes, unsigned entries)
{
    xrtnetrcoptions options;
    xrtnetrcerror error;
    xvalue* document = xrtNetrcParseMemory(text, strlen(text), NULL, &error);
    check(document != NULL, "node fixture", nodes, error.iCode);
    if (document == NULL) return;
    for (unsigned axis = 0; axis != 2; ++axis) {
        unsigned count = axis == 0 ? nodes : entries;
        for (unsigned limit = 0; limit <= count + 1; ++limit) {
            xmemdebugsnapshot before;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            xrtNetrcOptionsInit(&options);
            options.tLimits.iMaxNodes = axis == 0 ? limit : 0;
            options.tLimits.iMaxEntries = axis == 1 ? limit : 0;
            bool accepted = limit == 0 || limit >= count;
            xvalue* parsed = xrtNetrcParseMemory(text, strlen(text), &options, &error);
            check(accepted ? parsed != NULL && error.iCode == XRT_NETRC_OK :
                parsed == NULL && error.iCode == XRT_NETRC_ERROR_LIMIT,
                axis == 0 ? "parse node budget" : "parse entry budget", limit, error.iCode);
            xrtValueRelease(parsed);
            xbuffer* output = xrtNetrcStringify(document, &options, &error);
            check(accepted ? output != NULL && error.iCode == XRT_NETRC_OK :
                output == NULL && error.iCode == XRT_NETRC_ERROR_LIMIT,
                axis == 0 ? "write node budget" : "write entry budget", limit, error.iCode);
            xrtBufferDestroy(output);
            check(balanced(&before), "budget lifetime", limit, error.iCode);
        }
    }
    xrtValueRelease(document);
}

static void macro_case(const char* text, unsigned bytes, unsigned line, unsigned offset)
{
    xrtnetrcoptions options;
    xrtnetrcerror error;
    xvalue* document = xrtNetrcParseMemory(text, strlen(text), NULL, &error);
    check(document != NULL, "macro fixture", bytes, error.iCode);
    if (document == NULL) return;
    for (unsigned limit = bytes - 1; limit <= bytes + 1; ++limit) {
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        xrtNetrcOptionsInit(&options);
        options.tLimits.iMaxItemBytes = limit;
        bool accepted = limit >= bytes;
        xvalue* parsed = xrtNetrcParseMemory(text, strlen(text), &options, &error);
        check(accepted ? parsed != NULL && error.iCode == XRT_NETRC_OK :
            parsed == NULL && error.iCode == XRT_NETRC_ERROR_LIMIT &&
            error.iLine == line && error.iColumn == 1 && error.iOffset == offset,
            "parse macro byte budget", limit, error.iCode);
        xrtValueRelease(parsed);
        xbuffer* output = xrtNetrcStringify(document, &options, &error);
        check(accepted ? output != NULL && error.iCode == XRT_NETRC_OK :
            output == NULL && error.iCode == XRT_NETRC_ERROR_LIMIT,
            "write macro byte budget", limit, error.iCode);
        xrtBufferDestroy(output);
        check(balanced(&before), "macro lifetime", limit, error.iCode);
    }
    xrtValueRelease(document);
}

static void invalid_macro_utf8(void)
{
    static const unsigned char invalid[][4] = {
        {0x80, 0, 0, 0}, {0xc0, 0xaf, 0, 0}, {0xe2, 0x82, 0, 0},
        {0xed, 0xa0, 0x80, 0}, {0xf4, 0x90, 0x80, 0x80}
    };
    static const size_t sizes[] = {1, 2, 2, 3, 4};
    for (unsigned i = 0; i != sizeof(sizes) / sizeof(sizes[0]); ++i) {
        xrtnetrcerror error;
        xvalue* document = xrtNetrcParseMemory("macdef m\nvalid\n\n", 16, NULL, &error);
        if (document == NULL) {
            check(false, "invalid UTF-8 fixture", i, error.iCode);
            continue;
        }
        xvalue* macros = xrtValueObjectGet(document, xrtStrView("macros"));
        xvalue* lines = xrtValueArray();
        xvalue* value = xrtValueString((xstrview){(cstr)invalid[i], sizes[i]});
        bool ok = lines != NULL && value != NULL && xrtValueArrayAppendTake(lines, &value) &&
            xrtValueObjectSetTake(macros, xrtStrView("m"), &lines);
        check(ok, "invalid UTF-8 document", i, error.iCode);
        xrtValueRelease(value);
        xrtValueRelease(lines);
        if (ok) {
            xbuffer* output = xrtNetrcStringify(document, NULL, &error);
            check(output == NULL && error.iCode == XRT_NETRC_ERROR_UTF8,
                "write macro UTF-8 classification", i, error.iCode);
            xrtBufferDestroy(output);
        }
        xrtValueRelease(document);
    }
}

int main(void)
{
    if (!xrtMemDebugEnabled()) return 2;
    /* 预热错误槽，分配快照不把其一次性创建算成泄漏。 */
    xrtSetErrorInfo(XERR_STATE, "test.netrc", 0, "warm error slot");
    xrtClearError();
    node_case("", 0, 0);
    node_case("machine h\n", 1, 1);
    node_case("machine h login \"\"\n", 2, 1);
    node_case("machine h login u account a password p\n", 4, 1);
    node_case("default login guest password p\n", 3, 1);
    node_case("machine a login u\nmachine b login v password p\n", 5, 2);
    node_case("macdef m\n\n", 1, 1);
    node_case("macdef m\n one\n two\n\n", 3, 3);
    node_case("machine h login u\nmacdef m\n echo ok\n\n", 4, 3);
    macro_case("macdef m\n12345678\n\n", 8, 2, 9);
    macro_case("macdef m\r\n12345678\r\n\r\n", 8, 2, 10);
    macro_case("# note\nmacdef m\n12345678\n\n", 8, 3, 16);
    macro_case("\xef\xbb\xbfmacdef m\n12345678\n\n", 8, 2, 9);
    macro_case("macdef m\n\xe4\xbd\xa0\xe5\xa5\xbd" "123\n\n", 9, 2, 9);
    invalid_macro_utf8();
    printf("netrc resource budgets: %u checks, %u failures\n", checks, failures);
    return failures != 0;
}

#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#include "../xrt_netrc_writer.c"
