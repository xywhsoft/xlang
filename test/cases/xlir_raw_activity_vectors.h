/* Common saved-callback vectors. Both hosts call actual generated entries and
 * actual resident registry; the linker observers always delegate unchanged. */
#include "src/runtime/xl6_code_lease.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct RawCallbacks {
    int32_t (*integer)(void); bool (*boolean)(void); double (*real)(void);
    void* (*pointer)(void); void (*nothing)(void); int32_t (*text)(const char*);
} RawCallbacks;
static RawCallbacks callbacks[2];
static const XL6_CodeOwner* trackedOwner;
static _Thread_local unsigned depth;
static unsigned failureMode, probes;
static xatomic64 enters = {0}, leaves = {0}, ready = {0}, proceed = {0};
static bool observing, parking;
static void inspectActivity(bool, bool);
extern void __real_xl6CodeActivityEnterV1(XL6_CodeActivityV1*, const XL6_CodeOwner*);
extern void __real_xl6CodeActivityLeaveV1(XL6_CodeActivityV1*);
static void noAllocation(const xmemdebugsnapshot* before, bool failed)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->AllocCount == after.AllocCount && before->FreeCount == after.FreeCount);
    assert(before->ReallocCount == after.ReallocCount && failed == xrtMemDebugFailTriggered());
}
void __wrap_xl6CodeActivityEnterV1(XL6_CodeActivityV1* activity, const XL6_CodeOwner* owner)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); bool failed = xrtMemDebugFailTriggered();
    __real_xl6CodeActivityEnterV1(activity, owner); noAllocation(&before, failed);
    ++depth; xrtAtomic64FetchAdd(&enters, 1, XMEMORY_RELAXED);
    if (observing) {
        assert(depth == 1 && owner && owner->context == trackedOwner->context);
        inspectActivity(true, false);
        if (parking) {
            xrtAtomic64Store(&ready, 1, XMEMORY_RELEASE);
            while (!xrtAtomic64Load(&proceed, XMEMORY_ACQUIRE)) xrtThreadYield();
        }
    }
}
void __wrap_xl6CodeActivityLeaveV1(XL6_CodeActivityV1* activity)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); bool failed = xrtMemDebugFailTriggered();
    assert(depth); --depth; xrtAtomic64FetchAdd(&leaves, 1, XMEMORY_RELAXED);
    __real_xl6CodeActivityLeaveV1(activity); noAllocation(&before, failed);
}
void raw_activity_save(int32_t (*a)(void), bool (*b)(void), double (*c)(void), void* (*d)(void),
    void (*e)(void), int32_t (*f)(const char*), int32_t (*g)(void), bool (*h)(void),
    double (*i)(void), void* (*j)(void), void (*k)(void), int32_t (*l)(const char*))
{
    callbacks[0] = (RawCallbacks){a,b,c,d,e,f}; callbacks[1] = (RawCallbacks){g,h,i,j,k,l};
}
void raw_activity_probe(int64_t shape)
{
    assert(shape > 0 && shape < 7 && observing && depth == 1); ++probes;
    inspectActivity(true, false);
    if (failureMode) xl6ErrorScopePropagateFailure((xl6_error_result){false,
        failureMode == 2 ? xrtErrorCreate(XERR_STATE, "raw-activity", 163, "original raw failure") : NULL});
}
static void invoke(unsigned checked, unsigned shape, bool success)
{
    uint64_t before = xrtAtomic64Load(&enters, XMEMORY_RELAXED);
    RawCallbacks* c = &callbacks[checked];
    switch (shape) {
    case 0: assert(c->integer() == (success ? 42 : 0)); break;
    case 1: assert(c->boolean() == success); break;
    case 2: assert(c->real() == (success ? 4.25 : 0.0)); break;
    case 3: assert(c->pointer() == NULL); break;
    case 4: c->nothing(); break;
    case 5: assert(c->text("raw utf8 \xE4\xBD\xA0") == (success ? 17 : 0)); break;
    default: abort();
    }
    assert(xrtAtomic64Load(&enters, XMEMORY_RELAXED) == before + 1);
    assert(xrtAtomic64Load(&leaves, XMEMORY_RELAXED) == before + 1 && !depth);
}
static unsigned runVectors(void)
{
    unsigned vectors = 0;
    for (unsigned checked = 0; checked < 2; ++checked) for (unsigned mode = 0; mode < (checked ? 3u : 1u); ++mode)
        for (unsigned shape = 0; shape < 6; ++shape) {
            failureMode = mode; xl6_foreign_error_scope call = {0}; assert(xl6ForeignCallBegin(&call));
            invoke(checked, shape, !mode); xl6_foreign_error_result result = xl6ForeignCallEnd(&call);
            if (result.ok != !mode || (result.error != NULL) != (mode != 0))
                fprintf(stderr, "raw vector checked=%u mode=%u shape=%u ok=%d message=%s\n", checked, mode, shape,
                    result.ok, result.error ? xrtErrorMessage(result.error) : "none");
            assert(result.ok == !mode && (result.error != NULL) == (mode != 0));
            /* The established raw ABI supplies a fallback diagnostic when a
             * checked language result has none. Allocation failure below
             * proves that the independent failed status survives without it. */
            if (result.error) assert(!strcmp(xrtErrorMessage(result.error), mode == 2
                ? "original raw failure" : "language callback failed without an error"));
            xrtErrorFree(result.error); assert(!depth); inspectActivity(false, false); ++vectors;
        }
    failureMode = 0; return vectors;
}
static size_t runOom(void)
{
    size_t attempts = 0; failureMode = 2;
    for (unsigned shape = 0; shape < 6; ++shape) for (size_t point = 0; point < 128; ++point) {
        xl6_foreign_error_scope call = {0}; assert(xl6ForeignCallBegin(&call)); xl6ProductMemoryBegin(point);
        invoke(1, shape, false); bool hit = xl6ProductMemoryEnd(); ++attempts;
        xl6_foreign_error_result result = xl6ForeignCallEnd(&call); assert(!result.ok && !depth);
        if (!hit) assert(result.error && !strcmp(xrtErrorMessage(result.error), "original raw failure"));
        xrtErrorFree(result.error); inspectActivity(false, false); xl6ProductMemoryVerify();
        if (!hit) break;
        assert(point + 1 < 128);
    }
    failureMode = 0; return attempts;
}
static void directOom(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(0));
    for (unsigned shape = 0; shape < 6; ++shape) invoke(0, shape, true);
    noAllocation(&before, false); xrtMemDebugFailClear(); assert(!depth); inspectActivity(false, false);
}
static void missingDiagnosticOom(void)
{
    failureMode = 1;
    for (unsigned shape = 0; shape < 6; ++shape) {
        xl6_foreign_error_scope call = {0}; assert(xl6ForeignCallBegin(&call)); xl6ProductMemoryBegin(0);
        invoke(1, shape, false); assert(xl6ProductMemoryEnd());
        xl6_foreign_error_result result = xl6ForeignCallEnd(&call);
        assert(!result.ok && !result.error && !depth); inspectActivity(false, false); xl6ProductMemoryVerify();
    }
    failureMode = 0;
}
static int32 rawWorker(void* ignored)
{ (void)ignored; assert(callbacks[0].integer() == 42 && !depth); return 0; }
static void foreignCallback(void)
{
    parking = true; xthread* worker = xrtThreadCreate(rawWorker, NULL, 0); assert(worker);
    while (!xrtAtomic64Load(&ready, XMEMORY_ACQUIRE)) xrtThreadYield();
    inspectActivity(true, true); xrtAtomic64Store(&proceed, 1, XMEMORY_RELEASE);
    assert(xrtThreadWait(worker) == XWAIT_OK); xrtThreadDestroy(worker); parking = false;
    inspectActivity(false, false);
}
