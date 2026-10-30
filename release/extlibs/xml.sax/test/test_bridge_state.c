/* 每个 kind/budget 必须在新进程运行，不能把热线程槽当成冷启动。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include "../xml_sax_native.c"
#include "../../xml.dom/xml_dom_native.c"
#include "../../xml.etree/xml_etree_native.c"

static const char input[] = "<r/>";
static const char sentinel[] = "original target";
static unsigned failures;

static void check(bool condition, const char* message)
{
    if (!condition) { ++failures; fprintf(stderr, "XML bridge state: %s\n", message); }
}

/* 0..4 SAX，5..9 DOM，10..14 ElementTree；两个文本写出入口也单独验证。 */
static bool file_case(unsigned kind)
{
    return kind == 2 || kind == 6 || kind == 9 || kind == 11 || kind == 14;
}

static ptr prepare(unsigned kind)
{
    xrtxmlerror error;
    if (kind == 3 || kind == 4) return xrtXmlSaxOpenMemory(input, sizeof(input) - 1, NULL, &error);
    if (kind >= 7 && kind <= 9) return xrtXmlDomParseMemory(input, sizeof(input) - 1, NULL, &error);
    if (kind >= 12) return xrtXmlElementParseMemory(input, sizeof(input) - 1, NULL, &error);
    return NULL;
}

static void release_prepared(unsigned kind, ptr handle)
{
    if (kind == 3 || kind == 4) xrtXmlSaxDestroy(handle);
    if (kind >= 7 && kind <= 9) xrtXmlDomNodeRelease(handle);
    if (kind >= 12) xrtXmlElementRelease(handle);
}

static ptr invoke(unsigned kind, ptr handle, const char* path, bool* success)
{
    ptr result = NULL;
    switch (kind) {
    case 0: result = xsax_open_memory((void*)input, sizeof(input) - 1, true, true, false, false, 0, 0, 0, 0, 0, 0); break;
    case 1: result = xsax_open_text(input, sizeof(input) - 1, true, true, false, false, 0, 0, 0, 0, 0, 0); break;
    case 2: result = xsax_open_file(path, true, true, false, false, 0, 0, 0, 0, 0, 0); break;
    case 3: result = xsax_next(handle); break;
    case 4: *success = xsax_close(handle); return NULL;
    case 5: result = xdom_parse(input, sizeof(input) - 1, 0, 0, 0, 0, 0, 0); break;
    case 6: result = xdom_load(path, 0, 0, 0, 0, 0, 0); break;
    case 7: result = xdom_stringify(handle, false, 0, false, 1024); break;
    case 8: result = xdom_stringify_text(handle, false, 0, false, 1024); break;
    case 9: *success = xdom_save(handle, path, false, 0, false, 1024); return NULL;
    case 10: result = xetree_parse(input, sizeof(input) - 1, 0, 0, 0, 0, 0, 0); break;
    case 11: result = xetree_load(path, 0, 0, 0, 0, 0, 0); break;
    case 12: result = xetree_stringify(handle, false, 0, false, 1024); break;
    case 13: result = xetree_stringify_text(handle, false, 0, false, 1024); break;
    case 14: *success = xetree_save(handle, path, false, 0, false, 1024); return NULL;
    }
    *success = result != NULL && (kind != 3 || xrtValueType(result) != XVALUE_NULL);
    return result;
}

static void release_result(unsigned kind, ptr result)
{
    if (kind <= 2) xsax_destroy(result);
    else if (kind == 3) xrtValueRelease(result);
    else if (kind == 5 || kind == 6) xdom_release(result);
    else if (kind == 7 || kind == 12) xrtBufferDestroy(result);
    else if (kind == 8 || kind == 13) xrtFree(result);
    else if (kind == 10 || kind == 11) xetree_release(result);
}

static int32 check_thread(ptr context)
{
    unsigned id = *(unsigned*)context;
    char invalid[32] = "<r>";
    memset(invalid + 3, '\n', id + 1);
    strcpy(invalid + 4 + id, "</x>");
    for (unsigned round = 0; round < 12; ++round) {
        for (unsigned family = 0; family < 3; ++family) {
            unsigned kind = family == 0 ? 1 : family == 1 ? 5 : 10;
            bool success = false;
            if (round == 0) {
                if (!xrtMemDebugFailAfter(0)) return 1;
                ptr result = invoke(kind, NULL, NULL, &success);
                bool failed = xrtMemDebugFailTriggered() && !success && xrtGetError() &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY;
                xrtMemDebugFailClear();
                release_result(kind, result);
                xrtClearError();
                if (!failed) return 1;
            }
            ptr result;
            if (family == 0) {
                result = xsax_open_text(invalid, strlen(invalid), true, true, false, false, 0, 0, 0, 0, 0, 0);
                if (!result) return 1;
                for (;;) {
                    xvalue* event = xsax_next(result);
                    bool ended = !event || xrtValueType(event) == XVALUE_NULL;
                    xrtValueRelease(event);
                    if (ended) break;
                }
            } else if (family == 1) result = xdom_parse(invalid, strlen(invalid), 0, 0, 0, 0, 0, 0);
            else result = xetree_parse(invalid, strlen(invalid), 0, 0, 0, 0, 0, 0);
            bool rejected = xrtGetError() != NULL && (family == 0 || result == NULL);
            // 交错错误发布与读取；其他线程的行号不能覆盖当前线程。
            xrtThreadYield();
            int64 code = family == 0 ? xsax_error_code() : family == 1 ? xdom_error_code() : xetree_error_code();
            int64 line = family == 0 ? xsax_error_line() : family == 1 ? xdom_error_line() : xetree_error_line();
            if (family == 0) xsax_destroy(result);
            else release_result(kind, result);
            xrtClearError();
            if (!rejected || code != XRT_XML_ERROR_FORMAT || line != id + 2) return 1;
        }
    }
    return 0;
}

static int test_threads(void)
{
    xthread* threads[4] = {0};
    unsigned ids[] = {0, 1, 2, 3};
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
    if (__xsaxErrorKey) check(xrtThreadKeyDestroy(__xsaxErrorKey), "SAX key cleanup failed");
    if (__xdomErrorKey) check(xrtThreadKeyDestroy(__xdomErrorKey), "DOM key cleanup failed");
    if (__xetreeErrorKey) check(xrtThreadKeyDestroy(__xetreeErrorKey), "ElementTree key cleanup failed");
    if (ok && !failures) puts("XML bridge state threads: 4 workers, 144 diagnostics PASS");
    return ok && !failures ? 0 : 1;
}

int main(int argc, char** argv)
{
    if (argc > 1 && !strcmp(argv[1], "threads")) return test_threads();
    unsigned kind = argc > 1 ? (unsigned)atoi(argv[1]) : 5;
    unsigned budget = argc > 2 ? (unsigned)atoi(argv[2]) : 0;
    const char* path = argc > 3 ? argv[3] : NULL;
    bool save = kind == 9 || kind == 14;
    bool success = false;
    xmemdebugsnapshot before, after;
    if (kind > 14 || (file_case(kind) && (!path || xrtPathExists(path)))) return 2;
    if (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return 2;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    if (file_case(kind) && !xrtFileWriteAll(path, (xbytesview){
        (cbytes)(save ? sentinel : input), save ? sizeof(sentinel) - 1 : sizeof(input) - 1})) return 2;
    ptr handle = prepare(kind);
    if ((kind == 3 || kind == 4 || (kind >= 7 && kind <= 9) || kind >= 12) && !handle) return 2;
    if (!xrtMemDebugFailAfter(budget)) return 2;
    ptr result = invoke(kind, handle, path, &success);
    bool injected = xrtMemDebugFailTriggered();
    int error = xrtGetError() ? xrtErrorKind(xrtGetError()) : 0;
    xrtMemDebugFailClear();
    xthreadkey* key = kind <= 4 ? __xsaxErrorKey : kind <= 9 ? __xdomErrorKey : __xetreeErrorKey;
    bool stateReady = key && xrtThreadKeyGet(key) != NULL;
    check(injected ? !success && error == XERR_MEMORY : success && !error,
        "cold allocation failure was cleared or returned a successful result");
    release_result(kind, result);
    xrtClearError();

    /* 状态尚未准备好时，不能消费事件、关闭流或替换原目标。 */
    if (!stateReady && (kind == 3 || kind == 4)) {
        const xrtxmlevent* event;
        xrtxmlerror detail;
        check(xrtXmlSaxNext(handle, &event, &detail) == XRT_XML_NEXT_EVENT &&
            event && event->iType == XRT_XML_EVENT_START, "state failure consumed or closed the reader");
    }
    if (!stateReady && save) {
        size_t size = 0;
        bytes original = xrtFileReadAll(path, &size);
        check(original && size == sizeof(sentinel) - 1 && !memcmp(original, sentinel, size),
            "state failure changed the existing target");
        xrtFree(original);
    }
    release_prepared(kind, handle);
    xrtClearError();
    handle = prepare(kind);
    result = invoke(kind, handle, path, &success);
    check(success && !xrtGetError(), "same input did not recover in a fresh context");
    release_result(kind, result);
    release_prepared(kind, handle);
    if (file_case(kind)) check(xrtFileDelete(path), "could not remove owned fixture");
    xrtClearError();

    /* 单进程夹具已无存活使用者；显式释放三个桥接键，使首次初始化也可查平衡。 */
    if (__xsaxErrorKey) check(xrtThreadKeyDestroy(__xsaxErrorKey), "SAX key cleanup failed");
    if (__xdomErrorKey) check(xrtThreadKeyDestroy(__xdomErrorKey), "DOM key cleanup failed");
    if (__xetreeErrorKey) check(xrtThreadKeyDestroy(__xetreeErrorKey), "ElementTree key cleanup failed");
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount, "cold failure or recovery did not balance memory");
    if (!failures) printf("XML bridge state: kind=%u budget=%u injected=%u PASS\n", kind, budget, (unsigned)injected);
    return failures ? 1 : 0;
}

/* 独立 C 夹具；不能再链接这些实现的另一份对象。 */
#include "../../xml.dom/xrt_xml_dom_tree.c"
#include "../../xml.dom/xrt_xml_dom_parser.c"
#include "../../xml.dom/xrt_xml_dom_writer.c"
#include "../../xml.etree/xrt_xml_etree_tree.c"
#include "../../xml.etree/xrt_xml_etree_parser.c"
#include "../../xml.etree/xrt_xml_etree_writer.c"
#include "../xrt_xml_common.c"
#include "../xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
