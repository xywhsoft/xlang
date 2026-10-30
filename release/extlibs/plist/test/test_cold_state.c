/* 每个 kind/budget 使用新进程；预热后的预算不能覆盖首次线程槽创建。 */
#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include "../plist_native.c"

static const char xml[] = "<plist version='1.0'><string>value</string></plist>";
static const char sentinel[] = "original target";
static unsigned failures;

static void check(bool ok, const char* message)
{
    if (!ok) { ++failures; fprintf(stderr, "plist cold state: %s\n", message); }
}

static bool is_document(unsigned kind)
{
    return kind <= 3 || kind == 7 || kind == 8;
}

static ptr invoke(unsigned kind, xrtplistdocument* document, xbuffer* binary,
    const char* path, bool* success)
{
    ptr result = NULL;
    xbytesview input = binary ? xrtBufferView(binary) : (xbytesview){0};
    switch (kind) {
    case 0: result = xplist_create(); break;
    case 1: result = xplist_parse((ptr)xml, sizeof(xml) - 1, 0, 0, 0, 0, 0, 0, 0); break;
    case 2: result = xplist_parse_text(xml, sizeof(xml) - 1, 1, 0, 0, 0, 0, 0, 0); break;
    case 3: case 8: result = xplist_load(path, 0, 0, 0, 0, 0, 0, 0); break;
    case 4: case 9: result = xplist_stringify(document, kind == 4 ? 1 : 2, false, false, 0, 0, 0, 0, 0, 0); break;
    case 5: result = xplist_stringify_text(document, false, false, 0, 0, 0, 0, 0, 0); break;
    case 6: case 10: *success = xplist_save(document, path, kind == 6 ? 1 : 2, false, false, 0, 0, 0, 0, 0, 0); return NULL;
    case 7: result = xplist_parse((ptr)input.Data, input.Size, 0, 0, 0, 0, 0, 0, 0); break;
    }
    *success = result != NULL;
    return result;
}

static void release_result(unsigned kind, ptr result)
{
    if (is_document(kind)) xrtPlistDestroy(result);
    else if (kind == 5) xrtFree(result);
    else if (kind == 4 || kind == 9) xrtBufferDestroy(result);
}

static bool has_value(xrtplistdocument* document)
{
    xrtplistnode* root = xrtPlistRoot(document);
    return root && xrtPlistNodeType(root) == XRT_PLIST_STRING && xrtPlistByteSize(root) == 5 &&
        !memcmp(xrtPlistBytes(root), "value", 5);
}

/* 成功必须独立读回正确值；不能只证明两个写出器打印相同文本。 */
static bool verify(unsigned kind, ptr result, const char* path)
{
    if (kind == 0) return result && xrtPlistRoot(result) == NULL;
    if (is_document(kind)) return has_value(result);
    size_t size = 0;
    bytes file = NULL;
    xbytesview data;
    if (kind == 5) data = (xbytesview){(cbytes)result, strlen(result)};
    else if (kind == 4 || kind == 9) data = xrtBufferView(result);
    else { file = xrtFileReadAll(path, &size); data = (xbytesview){file, size}; }
    xrtplisterror error;
    xrtplistdocument* decoded = xrtPlistParseMemory(data.Data, data.Size, NULL, &error);
    bool ok = decoded && has_value(decoded) && error.iCode == XRT_PLIST_OK;
    xrtPlistDestroy(decoded);
    xrtFree(file);
    return ok;
}

static int32 check_thread(ptr context)
{
    unsigned id = *(unsigned*)context;
    if (!xrtMemDebugFailAfter(0)) return 1;
    ptr result = xplist_create();
    bool failed = !result && xrtMemDebugFailTriggered() && xrtGetError() &&
        xrtErrorKind(xrtGetError()) == XERR_MEMORY;
    xrtMemDebugFailClear();
    xplist_destroy(result);
    xrtClearError();
    if (!failed) return 1;
    xrtplistdocument* document = xrtPlistCreate();
    if (!document || !xrtPlistSetRoot(document, xrtPlistCreateNull(document))) {
        xrtPlistDestroy(document);
        return 1;
    }
    bool ok = true;
    static const int expected[] = {XRT_PLIST_ERROR_FORMAT, XRT_PLIST_ERROR_LIMIT,
        XRT_PLIST_ERROR_ARGUMENT, XRT_PLIST_ERROR_TYPE};
    for (unsigned round = 0; round < 16; ++round) {
        if (id == 0) result = xplist_parse_text("invalid", 7, 1, 0, 0, 0, 0, 0, 0);
        else if (id == 1) result = xplist_parse_text(xml, sizeof(xml) - 1, 1, 1, 0, 0, 0, 0, 0);
        else if (id == 2) result = xplist_parse_text(xml, sizeof(xml) - 1, 99, 0, 0, 0, 0, 0, 0);
        else result = xplist_stringify(document, 1, false, false, 0, 0, 0, 0, 0, 0);
        // 不同线程保留不同类别；交错发布与查询不能互相覆盖。
        xrtThreadYield();
        if (result || xplist_error_code() != expected[id]) ok = false;
        if (id == 3) xrtBufferDestroy(result);
        else xplist_destroy(result);
        xrtClearError();
    }
    xrtPlistDestroy(document);
    return ok ? 0 : 1;
}

static int test_threads(void)
{
    unsigned ids[] = {0, 1, 2, 3};
    xthread* threads[4] = {0};
    bool ok = true;
    if (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return 2;
    for (unsigned i = 0; i < 4; ++i) {
        threads[i] = xrtThreadCreate(check_thread, &ids[i], 0);
        if (!threads[i]) { ok = false; break; }
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (!threads[i]) continue;
        if (xrtThreadWait(threads[i]) != XWAIT_OK) return 1;
        if (xrtThreadExitCode(threads[i]) != 0) ok = false;
        xrtThreadDestroy(threads[i]);
    }
    if (__xplistErrorKey && !xrtThreadKeyDestroy(__xplistErrorKey)) ok = false;
    if (ok) puts("plist cold threads: 4 workers, 64 diagnostics PASS");
    return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
    if (argc > 1 && !strcmp(argv[1], "threads")) return test_threads();
    unsigned kind = argc > 1 ? (unsigned)atoi(argv[1]) : 0;
    unsigned budget = argc > 2 ? (unsigned)atoi(argv[2]) : 0;
    const char* path = argc > 3 ? argv[3] : NULL;
    bool save = kind == 6 || kind == 10;
    bool file = save || kind == 3 || kind == 8;
    if (kind > 10 || (file && (!path || xrtPathExists(path)))) return 2;
    if (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return 2;
    xrtClearError();
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    xrtplisterror detail;
    xrtplistdocument* document = xrtPlistParseMemory(xml, sizeof(xml) - 1, NULL, &detail);
    if (!document) return 2;
    xrtplistoptions options;
    xrtPlistOptionsInit(&options);
    options.iFormat = XRT_PLIST_FORMAT_BINARY;
    xbuffer* binary = xrtPlistStringify(document, &options, &detail);
    if (!binary) return 2;
    if (file) {
        xbytesview data = save ? (xbytesview){(cbytes)sentinel, sizeof(sentinel) - 1} :
            kind == 8 ? xrtBufferView(binary) : (xbytesview){(cbytes)xml, sizeof(xml) - 1};
        if (!xrtFileWriteAll(path, data)) return 2;
    }
    bool success = false;
    if (!xrtMemDebugFailAfter(budget)) return 2;
    ptr result = invoke(kind, document, binary, path, &success);
    bool injected = xrtMemDebugFailTriggered();
    bool memory = xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
    xrtMemDebugFailClear();
    bool stateReady = __xplistErrorKey && xrtThreadKeyGet(__xplistErrorKey) != NULL;
    // 已有结构化错误可延后给语言层；状态本身失败则必须保留 XRT 内存错误。
    if (!success && !memory) memory = xplist_error_code() == XRT_PLIST_ERROR_MEMORY;
    check(injected ? !success && memory : success && !xrtGetError(),
        "initial state failure was cleared or returned success");
    if (!injected && success) check(verify(kind, result, path), "successful output differs");
    release_result(kind, result);
    xrtClearError();
    if (!stateReady && save) {
        size_t size = 0;
        bytes original = xrtFileReadAll(path, &size);
        check(original && size == sizeof(sentinel) - 1 && !memcmp(original, sentinel, size),
            "state initialization failure changed the target");
        xrtFree(original);
    }
    result = invoke(kind, document, binary, path, &success);
    check(success && !xrtGetError() && xplist_error_code() == XRT_PLIST_OK,
        "same input did not recover");
    if (success) check(verify(kind, result, path), "recovery changed the value");
    release_result(kind, result);
    xrtBufferDestroy(binary);
    xrtPlistDestroy(document);
    if (file) check(xrtFileDelete(path), "owned fixture cleanup failed");
    xrtClearError();
    // 测试进程没有其他使用者，释放键本身以检查首次创建的全部活动分配。
    if (__xplistErrorKey) check(xrtThreadKeyDestroy(__xplistErrorKey), "thread key cleanup failed");
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount, "failure or recovery did not balance memory");
    if (!failures) printf("plist cold state: kind=%u budget=%u injected=%u PASS\n", kind, budget, (unsigned)injected);
    return failures ? 1 : 0;
}

/* 独立 C 夹具；只再链接一份 XRT，不重复链接下列实现。 */
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
