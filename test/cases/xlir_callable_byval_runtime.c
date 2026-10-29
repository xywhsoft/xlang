#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_STRING
#define XRT_MODULE_VALUE_CONTAINER
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int bridge_init(void);
extern void bridge_fini(void);
extern xvalue* bridge_error_value(xerror*);
extern bool bridge_forward(int, xvalue**, size_t, int64_t*);
extern bool bridge_reverse(int, xstrview, xvalue*, xerror*, int64_t*);
static unsigned calls;
static xerror* bodyError;
static xerror* cleanupError;
static unsigned drops;
void bridge_count(void) { ++calls; }
void bridge_raise(void) { xrtSetError(bodyError); }
static void raising_drop(void* handle, void* data)
{
    assert(handle == &drops && data == NULL); ++drops; xrtSetError(cleanupError);
}
static const xvaluehandleops handleOps = {NULL, raising_drop, NULL, NULL};
static void balanced(const xmemdebugsnapshot* before, unsigned kind, unsigned budget)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    if (before->LiveCount != after.LiveCount || before->LiveBytes != after.LiveBytes)
        fprintf(stderr, "callable BYVAL kind=%u budget=%u live=%lld bytes=%lld\n", kind, budget,
            (long long)(after.LiveCount - before->LiveCount), (long long)(after.LiveBytes - before->LiveBytes));
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    unsigned round, kind, budget, faults = 0, executions = 0;
    xmemdebugsnapshot initial, before, final;
    xerror* sentinel;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    sentinel = xrtErrorCreate(XERR_STATE, "bridge", 88, "original body error"); assert(sentinel);
    bodyError = sentinel;
    cleanupError = xrtErrorCreate(XERR_STATE, "bridge", 99, "secondary cleanup error"); assert(cleanupError);
    assert(bridge_init());
    for (round = 0; round < (smoke ? 1u : 20u); ++round)
    for (kind = 0; kind < 12; ++kind) {
        for (budget = 0; budget < 48; ++budget) {
            bool ok; int triggered; int64_t result = 0;
            unsigned oldCalls = calls;
            unsigned oldDrops = drops;
            int forward = kind < 2 || kind == 6 || kind == 7;
            xvalue* arguments[3] = {0}; char* text = NULL; xvalue* value = NULL; xerror* issue = NULL;
            if (!smoke) xrtMemDebugSnapshot(&before);
            if (forward) {
                arguments[0] = xrtValueString(xrtStrView("text"));
                arguments[1] = xrtValueArray();
                arguments[2] = kind == 6 ? xrtValueObject() : bridge_error_value(sentinel);
                assert(arguments[0] && arguments[1] && arguments[2]);
            } else {
                text = xrtStrDup("text"); issue = xrtErrorRef(sentinel);
                if (kind >= 8) {
                    void* handle = &drops; value = xrtValueHandleTake(&handle, &handleOps, NULL); assert(!handle);
                } else value = xrtValueArray();
                assert(text && value && issue);
            }
            if (!smoke) assert(xrtMemDebugFailAfter(budget));
            ok = forward
                ? bridge_forward((int)kind, arguments, kind == 7 ? 2 : 3, &result)
                : bridge_reverse((int)kind, (xstrview){text, strlen(text)}, value, issue, &result);
            triggered = !smoke && xrtMemDebugFailTriggered();
            if (!smoke) xrtMemDebugFailClear();
            ++executions;
            if (triggered) {
                ++faults;
                if (ok) fprintf(stderr, "BYVAL kind=%u budget=%u unexpectedly succeeded\n", kind, budget);
                assert(!ok);
                /* Invalid environment/output fails before invoking native code.
                 * If creating that diagnostic itself fails, the explicit false
                 * outcome is authoritative; cleanup must not replace it with
                 * the secondary destructor diagnostic (kinds 10 and 11). */
                if (!xrtGetError())
                    assert(budget == 0 && (kind == 4 || kind == 5 || kind == 10 || kind == 11));
            }
            else if (kind == 0 || kind == 2) assert(ok && result == 42 && !xrtGetError() && calls == oldCalls + 1);
            else {
                const xerror* failure = xrtGetError();
                assert(!ok && failure);
                if (kind == 1 || kind == 3 || kind >= 8) {
                    while (xrtErrorCause(failure)) failure = xrtErrorCause(failure);
                    if (kind == 8) assert(xrtErrorCode(failure) == 99 && calls == oldCalls + 1);
                    else if (kind >= 10) assert(xrtErrorCode(failure) != 99 && calls == oldCalls);
                    else assert(xrtErrorCode(failure) == 88 && calls == oldCalls + 1);
                } else assert(calls == oldCalls);
            }
            if (!ok) assert(result == 0); /* Failed bridges never commit a result. */
            if (kind == 9 && calls != oldCalls) {
                const xerror* failure = xrtGetError(); assert(failure);
                while (xrtErrorCause(failure)) failure = xrtErrorCause(failure);
                assert(xrtErrorCode(failure) == 88); /* Cleanup may not mask a body failure. */
            }
            xrtClearError();
            assert(drops == oldDrops + (kind >= 8 ? 1u : 0u));
            if (forward) {
                xrtValueRelease(arguments[2]); xrtValueRelease(arguments[1]); xrtValueRelease(arguments[0]);
            }
            assert(!xrtGetError());
            if (!smoke) balanced(&before, kind, budget);
            if (!triggered) break;
        }
        assert(budget < 48);
    }
    bridge_fini(); xrtErrorFree(sentinel); xrtErrorFree(cleanupError); assert(!xrtGetError());
    if (!smoke) {
        balanced(&initial, 99, 0); xrtMemDebugSnapshot(&final);
        printf("callable BYVAL: %u executions, %u allocation faults, %llu balanced allocations; forward/reverse, malformed frames, invalid environment/output, primary errors, raising destructors\n",
            executions, faults, (unsigned long long)(final.AllocCount - initial.AllocCount));
    } else puts("callable BYVAL generated C-host smoke passed");
    return 0;
}
