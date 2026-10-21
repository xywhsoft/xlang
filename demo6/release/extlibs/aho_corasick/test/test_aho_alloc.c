#define XRT_MODULE_MEMORY_DEBUG
#include "../include/aho_corasick_native.h"
#include <assert.h>
#include <stdio.h>

/* 遍历真实 XRT 分配点，覆盖核心、桥接句柄和返回 Buffer，而非只注入某个 malloc。 */
static bool workload(void)
{
    void* ac = xac_create(0, 0, 0, 0, 0, 0, 0, 0);
    void* result = NULL;
    void* scan = NULL;
    xbuffer* bytes = NULL;
    bool ok = false;
    if (!ac || !xac_add_pattern(ac, "he", 2) || !xac_add_pattern(ac, "she", 3) ||
        !xac_add_pattern(ac, "hers", 4) || !xac_build(ac)) goto done;
    result = xac_search(ac, "ushers", 6);
    if (!result) goto done;
    bytes = xac_result_pattern(result, 0);
    if (!bytes) goto done;
    xrtBufferDestroy(bytes); bytes = NULL;
    xac_result_destroy(result); result = xac_find(ac, "ushers", 6);
    if (!result) goto done;
    xac_result_destroy(result); result = xac_find(ac, "none", 4);
    if (!result) goto done;
    scan = xac_scan_begin(ac, "ushers", 6);
    if (!scan || xac_scan_next(scan) != 1) goto done;
    xac_scan_end(scan); scan = xac_stream_create(ac);
    if (!scan || !xac_stream_feed(scan, "ushers", 6) || xac_stream_next(scan) != 1) goto done;
    bytes = xac_stream_pattern(scan);
    if (!bytes) goto done;
    xrtBufferDestroy(bytes); bytes = xac_replace(ac, "ushers", 6, "*", 1);
    if (!bytes) goto done;
    xrtBufferDestroy(bytes); bytes = xac_mask(ac, "ushers", 6, "*", 1);
    if (!bytes) goto done;
    xrtBufferDestroy(bytes); bytes = xac_highlight(ac, "ushers", 6, "<", 1, ">", 1);
    if (!bytes) goto done;
    xrtBufferDestroy(bytes); bytes = xrtBufferCreate();
    if (!bytes || !xac_output_append(ac, bytes, "output", 6, 0, 6)) goto done;
    ok = true;
done:
    xrtBufferDestroy(bytes);
    xac_result_destroy(result);
    xac_scan_end(scan);
    xac_destroy(ac);
    return ok;
}

static void clean(void)
{
    xmemdebugsnapshot s;
    xrtClearError();
    xrtMemDebugSnapshot(&s);
    assert(s.LiveCount == 0 && s.LiveBytes == 0);
    assert(s.OverflowCount == 0 && s.UnderflowCount == 0 && s.DoubleFreeCount == 0);
    assert(s.InvalidFreeCount == 0 && s.UseAfterFreeCount == 0);
}

int main(void)
{
    assert(xrtMemDebugEnable(true));
    unsigned failed = 0;
    bool complete = false;
    for (unsigned i = 0; i < 256; ++i) {
        assert(xrtMemDebugFailAfter(i));
        bool ok = workload();
        bool injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) { assert(!ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++failed; }
        else { assert(ok); complete = true; }
        clean();
        assert(workload());
        clean();
        if (complete) break;
    }
    assert(complete && failed > 20);
    assert(xrtMemDebugEnable(false));
    printf("aho allocator: %u injected points, clean recovery passed\n", failed);
    return 0;
}
