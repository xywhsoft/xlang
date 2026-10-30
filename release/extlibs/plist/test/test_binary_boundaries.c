/* 覆盖各类 binary 节点的真实 OOM、资源分类与首因，不只测同一个字符串。 */
#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/xrt_plist.h"
#include <stdio.h>
#include <string.h>

static unsigned failures, faultPoints, limitCases, malformedCases;
static unsigned currentKind, currentBudget;
static const unsigned char raw[] = {0, 0x80, 0xff};

static void check(bool ok, const char* message)
{
    if (!ok) {
        ++failures;
        fprintf(stderr, "plist binary: kind=%u budget=%u %s\n", currentKind, currentBudget, message);
    }
}

static xrtplistdocument* fixture(unsigned kind)
{
    xrtplistdocument* document = xrtPlistCreate();
    xrtplistnode* root = NULL;
    bool ok = document != NULL;
    if (!ok) return NULL;
    switch (kind) {
    case 0: root = xrtPlistCreateNull(document); break;
    case 1: root = xrtPlistCreateBool(document, true); break;
    case 2: root = xrtPlistCreateInt(document, -42); break;
    case 3: root = xrtPlistCreateReal(document, 1.25); break;
    case 4: root = xrtPlistCreateString(document, (str)"ascii"); break;
    case 5: root = xrtPlistCreateString(document, (str)"你好🎶"); break;
    case 6: root = xrtPlistCreateData(document, raw, sizeof(raw)); break;
    case 7: root = xrtPlistCreateDate(document, INT64_C(978307201000000)); break;
    case 8: root = xrtPlistCreateUid(document, UINT64_MAX); break;
    case 9:
        root = xrtPlistCreateArray(document);
        ok = root && xrtPlistArrayAppend(root, xrtPlistCreateInt(document, 1)) &&
            xrtPlistArrayAppend(root, xrtPlistCreateBool(document, true));
        break;
    case 10:
        root = xrtPlistCreateDict(document);
        ok = root && xrtPlistDictSet(root, "alpha", 5, xrtPlistCreateInt(document, 1)) &&
            xrtPlistDictSet(root, "beta", 4, xrtPlistCreateString(document, (str)"你好"));
        break;
    case 11: {
        root = xrtPlistCreateDict(document);
        xrtplistnode* array = xrtPlistCreateArray(document);
        xrtplistnode* child = xrtPlistCreateDict(document);
        ok = root && array && child && xrtPlistDictSet(root, "items", 5, array) &&
            xrtPlistArrayAppend(array, xrtPlistCreateString(document, (str)"a")) &&
            xrtPlistArrayAppend(array, child) &&
            xrtPlistDictSet(child, "n", 1, xrtPlistCreateInt(document, 2));
        break;
    }
    }
    if (!ok || !root || !xrtPlistSetRoot(document, root)) {
        xrtPlistDestroy(document);
        return NULL;
    }
    return document;
}

static bool text_is(xrtplistnode* node, const char* text)
{
    size_t size = strlen(text);
    return node && xrtPlistNodeType(node) == XRT_PLIST_STRING &&
        xrtPlistByteSize(node) == size && !memcmp(xrtPlistBytes(node), text, size);
}

static bool correct(unsigned kind, xrtplistdocument* document)
{
    static const int types[] = {0, 1, 2, 3, 4, 4, 5, 6, 9, 7, 8, 8};
    xrtplistnode* root = xrtPlistRoot(document);
    if (!root || xrtPlistNodeType(root) != types[kind]) return false;
    switch (kind) {
    case 0: return true;
    case 1: return xrtPlistBool(root);
    case 2: return xrtPlistInt(root) == -42;
    case 3: return xrtPlistReal(root) == 1.25;
    case 4: return text_is(root, "ascii");
    case 5: return text_is(root, "你好🎶");
    case 6: return xrtPlistByteSize(root) == sizeof(raw) && !memcmp(xrtPlistBytes(root), raw, sizeof(raw));
    case 7: return xrtPlistDate(root) == INT64_C(978307201000000);
    case 8: return xrtPlistUid(root) == UINT64_MAX;
    case 9: return xrtPlistArrayCount(root) == 2 && xrtPlistInt(xrtPlistArrayGet(root, 0)) == 1 &&
        xrtPlistBool(xrtPlistArrayGet(root, 1));
    case 10: return xrtPlistDictCount(root) == 2 && xrtPlistInt(xrtPlistDictGet(root, "alpha", 5)) == 1 &&
        text_is(xrtPlistDictGet(root, "beta", 4), "你好");
    case 11: {
        xrtplistnode* array = xrtPlistDictGet(root, "items", 5);
        return array && xrtPlistArrayCount(array) == 2 && text_is(xrtPlistArrayGet(array, 0), "a") &&
            xrtPlistInt(xrtPlistDictGet(xrtPlistArrayGet(array, 1), "n", 1)) == 2;
    }
    }
    return false;
}

static void allocation_cases(unsigned kind, xbuffer* binary)
{
    xbytesview input = xrtBufferView(binary);
    bool complete = false;
    for (unsigned budget = 0; budget < 128; ++budget) {
        currentBudget = budget;
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        check(xrtMemDebugFailAfter(budget), "cannot inject allocation failure");
        xrtplisterror error;
        xrtplistdocument* parsed = xrtPlistParseMemory(input.Data, input.Size, NULL, &error);
        bool injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            ++faultPoints;
            check(!parsed && error.iCode == XRT_PLIST_ERROR_MEMORY, "OOM became FORMAT or a successful document");
            check(!xrtGetError() || xrtErrorKind(xrtGetError()) == XERR_MEMORY,
                "null-root handoff overwrote the original memory error");
        } else check(parsed && error.iCode == XRT_PLIST_OK && correct(kind, parsed), "binary value differs");
        xrtPlistDestroy(parsed);
        xrtClearError();
        parsed = xrtPlistParseMemory(input.Data, input.Size, NULL, &error);
        check(parsed && error.iCode == XRT_PLIST_OK && correct(kind, parsed), "same source did not recover");
        xrtPlistDestroy(parsed);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
            before.UseAfterFreeCount == after.UseAfterFreeCount, "failure or recovery did not balance memory");
        if (!injected) { complete = true; break; }
    }
    check(complete, "allocation scan did not finish");

    // 当前输入的标记故意改成未支持类型；旧的 MEMORY/IO 不能改变格式判定。
    uint8* copy = xrtMalloc(input.Size);
    if (!copy) { check(false, "cannot prepare malformed control"); return; }
    memcpy(copy, input.Data, input.Size);
    copy[8] = 0x70;
    for (unsigned cause = 0; cause < 3; ++cause) {
        xrtClearError();
        if (cause) xrtSetErrorInfo(cause == 1 ? XERR_IO : XERR_MEMORY, "test.old", 7, "stale cause");
        xrtplisterror error;
        xrtplistdocument* parsed = xrtPlistParseMemory(copy, input.Size, NULL, &error);
        check(!parsed && error.iCode == XRT_PLIST_ERROR_FORMAT && error.iOffset == 8,
            "malformed marker was relabeled by a stale cause");
        check(!xrtGetError(), "failed root created an unrelated argument error");
        xrtPlistDestroy(parsed);
        xrtClearError();
        ++malformedCases;
    }
    xrtFree(copy);
}

static void entry_limits(unsigned kind, xrtplistdocument* source)
{
    static const uint64 limits[] = {1, 2, 3, UINT64_MAX / 2, UINT64_MAX / 2 + 1,
        UINT64_MAX / 2 + 2, UINT64_MAX, 0};
    for (int format = 1; format <= 2; ++format) {
        xrtplistoptions options;
        xrtplisterror error;
        xrtPlistOptionsInit(&options);
        options.iFormat = format;
        xbuffer* encoded = xrtPlistStringify(source, &options, &error);
        if (!encoded) { check(false, "cannot encode limit fixture"); return; }
        for (size_t i = 0; i < sizeof(limits) / sizeof(*limits); ++i) {
            xrtPlistOptionsInit(&options);
            options.tLimits.iMaxEntries = limits[i];
            xrtplistdocument* parsed = xrtPlistParseMemory(encoded->Data, encoded->Size, &options, &error);
            check(limits[i] == 1 ? !parsed && error.iCode == XRT_PLIST_ERROR_LIMIT :
                parsed && error.iCode == XRT_PLIST_OK && correct(kind, parsed),
                "container limit was FORMAT or a large limit wrapped");
            xrtPlistDestroy(parsed);
            xrtClearError();
            ++limitCases;
        }
        xrtBufferDestroy(encoded);
    }
}

int main(int argc, char** argv)
{
    if (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return 2;
    for (unsigned kind = 0; kind < 12; ++kind) {
        currentKind = kind;
        xrtplistdocument* source = fixture(kind);
        if (!source) return 2;
        xrtplistoptions options;
        xrtplisterror error;
        xrtPlistOptionsInit(&options);
        options.iFormat = XRT_PLIST_FORMAT_BINARY;
        xbuffer* encoded = xrtPlistStringify(source, &options, &error);
        if (!encoded) return 2;
        if (argc > 1) {
            char path[4096];
            int size = snprintf(path, sizeof(path), "%s/vector-%u.bplist", argv[1], kind);
            check(size > 0 && (size_t)size < sizeof(path) && !xrtPathExists(path) &&
                xrtFileWriteAll(path, xrtBufferView(encoded)), "cannot write owned oracle fixture");
        }
        allocation_cases(kind, encoded);
        if (kind >= 9) entry_limits(kind, source);
        xrtBufferDestroy(encoded);
        xrtPlistDestroy(source);
    }
    if (!failures) printf("plist binary: 12 values, %u OOM points, %u limits, %u malformed PASS\n",
        faultPoints, limitCases, malformedCases);
    return failures ? 1 : 0;
}

#include "../xrt_plist_tree.c"
#include "../xrt_plist_io.c"
#include "../xrt_plist_xml.c"
#include "../xrt_plist_binary.c"
#include "../xrt_plist_xml_writer.c"
#include "../xrt_plist_binary_writer.c"
#include "../../xml.dom/xrt_xml_dom_tree.c"
#include "../../xml.dom/xrt_xml_dom_parser.c"
#include "../../xml.dom/xrt_xml_dom_writer.c"
#include "../../xml.sax/xrt_xml_common.c"
#include "../../xml.sax/xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
