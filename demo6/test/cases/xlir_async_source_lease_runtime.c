#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_STRING
#define XRT_MODULE_VALUE
#define XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE_CONTINUE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void source_lease_prepare(void), source_lease_finish(void), source_lease_produce(xpromise*, unsigned, void*, xerror*);
extern void source_lease_release_external(xfuture*, xpromise*, int);
extern xfuture* source_lease_launch(int, xfuture*);
extern xfuture* source_lease_fault_launch(int, xfuture*);
extern xvalue* source_lease_new(void);
extern void source_lease_entry_probe(char*, char*, xvalue*, char*, int);
static int kind, hold, dropMode, bodyMode, finalMode, ambientMode, smoke;
static unsigned drops, boxes, bodyCount, finallyCount, observations, executions;
static int faultIndex = -1;
static unsigned faults, postArmAllocations;
static xmemdebugsnapshot armedAt;
static xfuture* temporary;
static xerror* inputError;
static int payload_kind(void) { return kind >= 5 ? kind - 3 : kind; }
static int temporary_kind(void) { return kind == 1 || kind >= 5; }
void source_lease_fault_arm(void)
{
    xrtMemDebugSnapshot(&armedAt);
    if (faultIndex >= 0) assert(xrtMemDebugFailAfter((uint64)faultIndex));
}
void source_lease_fault_disarm(void)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    postArmAllocations = (unsigned)(after.AllocCount - armedAt.AllocCount);
    if (faultIndex >= 0) { assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++faults; }
}
void source_lease_report_context(void)
{
    fprintf(stderr, "source lease context: kind=%d hold=%d drop=%d body=%d finally=%d ambient=%d drops=%u bodyCount=%u finallyCount=%u\n",
        kind, hold, dropMode, bodyMode, finalMode, ambientMode, drops, bodyCount, finallyCount);
}
int source_lease_ambient(void) { return ambientMode; }
int source_lease_body_mode(void) { ++bodyCount; return bodyMode; }
int source_lease_finally_mode(void) { ++finallyCount; return finalMode; }
int source_lease_drop_mode(void) { return dropMode; }
void source_lease_box_drop(void) { ++boxes; }
xfuture* source_lease_input(void) { xfuture* result = temporary; temporary = NULL; assert(result); return result; }
xfuture* source_lease_string(void) { return source_lease_input(); }
xfuture* source_lease_object(void) { return source_lease_input(); }
xfuture* source_lease_nested(void) { return source_lease_input(); }
static void free_inner(void* value, void* data) { (void)data; xrtFree(value); }
static void* payload_new(void)
{
    void* result;
    if (payload_kind() == 2) {
        char** p = (char**)xrtMalloc(sizeof(*p)); assert(p); *p = xrtStrDup("source lease text"); assert(*p); result = p;
    } else if (payload_kind() == 3) {
        xvalue** p = (xvalue**)xrtMalloc(sizeof(*p)); assert(p); *p = source_lease_new(); assert(*p); result = p;
    } else if (payload_kind() == 4) {
        xfuture** p = (xfuture**)xrtMalloc(sizeof(*p)); xpromise* promise; int64_t* value = (int64_t*)xrtMalloc(sizeof(*value));
        assert(p && value); *value = 10; *p = NULL; promise = xrtPromiseCreate(p, NULL); assert(promise && *p);
        assert(xrtPromiseResolveOwned(promise, value, free_inner, NULL)); xrtPromiseDestroy(promise); result = p;
    } else {
        int64_t* p = (int64_t*)xrtMalloc(sizeof(*p)); assert(p); *p = 10; result = p;
    }
    return result;
}
void source_lease_payload_destroy(void* value)
{
    ++drops;
    if (payload_kind() == 2) xrtFree(*(char**)value);
    else if (payload_kind() == 3) xrtValueRelease(*(xvalue**)value);
    else if (payload_kind() == 4) xrtFutureDestroy(*(xfuture**)value);
    xrtFree(value);
}
static void observe(const xfutureresult* input, xpromise* output, void* data)
{
    unsigned expectedDrops = *(unsigned*)data;
    assert(input && drops == expectedDrops && finallyCount == 1); ++observations;
    assert(xrtPromiseResolve(output, NULL));
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount && after.InvalidFreeCount == before->InvalidFreeCount && after.UseAfterFreeCount == before->UseAfterFreeCount);
}
static void run_case(unsigned state)
{
    xfuture *input = NULL, *external = NULL, *output, *observed; xpromise* promise; xfutureresult outcome;
    void* value = state == 0 ? payload_new() : NULL; unsigned expectedDrops = hold == 0 && state == 0 ? 1u : 0u;
    int effective = (int)state, code = state == 1 ? 88 : 0, internal = 0;
    int bodyReached = state == 0 && !(temporary_kind() && hold == 0 && dropMode);
    drops = boxes = bodyCount = finallyCount = 0;
    promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
    if (hold != 2) { source_lease_produce(promise, state, value, inputError); xrtPromiseDestroy(promise); promise = NULL; }
    if (hold == 1) external = xrtFutureRef(input);
    if (temporary_kind()) temporary = input;
    output = source_lease_launch(kind, input); assert(output && !temporary && !xrtGetError());
    observed = xrtFutureContinue(output, observe, &expectedDrops); assert(observed);
    if (hold == 2) { assert(!xrtFutureDone(output)); source_lease_produce(promise, state, value, inputError); }
    if (temporary_kind() && hold == 0 && state == 0 && dropMode) { effective = 1; code = 93; internal = dropMode == 2; }
    if (bodyReached && bodyMode) { effective = 1; code = 73; internal = bodyMode == 2; }
    if (finalMode) { effective = 1; code = 77; internal = finalMode == 2; }
    if (!effective && hold == 0 && dropMode) { effective = 1; code = 93; internal = dropMode == 2; }
    assert(xrtFutureResult(output, &outcome) && !xrtGetError());
    if (outcome.State != (effective == 0 ? XFUTURE_RESOLVED : effective == 1 ? XFUTURE_FAILED : effective == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED) ||
        bodyCount != (unsigned)bodyReached || finallyCount != 1)
        fprintf(stderr, "source lease mismatch: kind=%d hold=%d input=%u drop=%d body=%d finally=%d ambient=%d state=%d effective=%d bodyCount=%u expectedBody=%d finallyCount=%u code=%d\n",
            kind, hold, state, dropMode, bodyMode, finalMode, ambientMode, outcome.State, effective, bodyCount, bodyReached, finallyCount, outcome.Error ? xrtErrorCode(outcome.Error) : 0);
    assert(outcome.State == (effective == 0 ? XFUTURE_RESOLVED : effective == 1 ? XFUTURE_FAILED : effective == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED));
    assert(bodyCount == (unsigned)bodyReached && finallyCount == 1 && drops == expectedDrops);
    if (effective == 1) { assert(outcome.Error && !outcome.Value); if (internal) assert(xrtErrorKind(outcome.Error) == XERR_INTERNAL); else assert(xrtErrorCode(outcome.Error) == code); }
    if (!effective) {
        assert(outcome.Value && !outcome.Error);
        if (payload_kind() == 2) assert(!strcmp(*(char**)outcome.Value, "source lease text"));
        else if (payload_kind() == 3) assert(*(xvalue**)outcome.Value && boxes == 0);
        else assert(*(int64_t*)outcome.Value == 17);
    }
    assert(xrtFutureState(observed) == XFUTURE_RESOLVED);
    xrtFutureDestroy(observed); xrtFutureDestroy(output);
    source_lease_release_external(external, promise, hold != 0 && state == 0 ? dropMode : 0);
    assert(drops == (state == 0 ? 1u : 0u) && boxes == (payload_kind() == 3 && state == 0 ? 1u : 0u) && !xrtGetError());
    ++executions;
}
static unsigned fault_case(int fault)
{
    xfuture *input = NULL, *output; xpromise* promise; xfutureresult outcome; void* value;
    xmemdebugsnapshot before; unsigned allocations;
    xrtMemDebugSnapshot(&before); drops = boxes = bodyCount = finallyCount = 0;
    hold = bodyMode = finalMode = 0; value = payload_new(); promise = xrtPromiseCreate(&input, NULL);
    assert(promise && input); source_lease_produce(promise, 0, value, inputError); xrtPromiseDestroy(promise);
    if (temporary_kind()) temporary = input;
    faultIndex = fault; output = source_lease_fault_launch(kind, input); allocations = postArmAllocations; faultIndex = -1;
    if (output) {
        assert(xrtFutureResult(output, &outcome));
        if (fault >= 0) {
            if (outcome.State != XFUTURE_FAILED || !outcome.Error || xrtErrorKind(outcome.Error) != XERR_MEMORY || outcome.Value) {
                source_lease_report_context();
                fprintf(stderr, "source lease allocation fault: index=%d state=%d errorKind=%d code=%d\n", fault, outcome.State,
                    outcome.Error ? (int)xrtErrorKind(outcome.Error) : -1, outcome.Error ? xrtErrorCode(outcome.Error) : 0);
            }
            assert(outcome.State == XFUTURE_FAILED && outcome.Error && xrtErrorKind(outcome.Error) == XERR_MEMORY && !outcome.Value);
        }
        else if (dropMode) {
            assert(outcome.State == XFUTURE_FAILED && outcome.Error && !outcome.Value);
            if (dropMode == 1) assert(xrtErrorCode(outcome.Error) == 93);
            else assert(xrtErrorKind(outcome.Error) == XERR_INTERNAL);
        } else assert(outcome.State == XFUTURE_RESOLVED && outcome.Value && !strcmp(*(char**)outcome.Value, "source lease text"));
        xrtFutureDestroy(output);
    } else assert(fault >= 0);
    /* A temporary source is not evaluated if launch allocation fails. Its
     * still-external owner must be released by this caller, not by the callee. */
    if (temporary) { source_lease_release_external(temporary, NULL, dropMode); temporary = NULL; }
    assert(drops == 1 && !xrtGetError()); balanced(&before); return allocations;
}
static unsigned entry_fault_case(int present, int fault)
{
    xmemdebugsnapshot before; unsigned allocations;
    char *first, *second, *last; xvalue* owned;
    xrtMemDebugSnapshot(&before); boxes = bodyCount = 0; bodyMode = finalMode = dropMode = 0;
    first = xrtStrDup("first"); second = present ? xrtStrDup("second") : NULL; last = xrtStrDup("last");
    owned = source_lease_new(); assert(first && (!present || second) && last && owned);
    faultIndex = fault; source_lease_entry_probe(first, second, owned, last, fault >= 0);
    allocations = postArmAllocations; faultIndex = -1;
    assert(boxes == 1 && bodyCount == (fault < 0 ? 1u : 0u));
    /* Failed adaptation must never destroy or mutate the caller's borrows. */
    assert(!strcmp(first, "first") && (!present || !strcmp(second, "second")) && !strcmp(last, "last"));
    xrtFree(first); xrtFree(second); xrtFree(last); balanced(&before); return allocations;
}
int main(int argc, char** argv)
{
    unsigned round, state; xmemdebugsnapshot before, after;
    smoke = argc == 2 && !strcmp(argv[1], "--smoke"); assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&before);
    inputError = xrtErrorCreate(XERR_STATE, "source lease", 88, "input failed"); assert(inputError); source_lease_prepare();
    for (round = 0; round < (smoke ? 1u : 3u); ++round) for (kind = 0; kind < 8; ++kind)
    for (hold = 0; hold < 3; ++hold) for (state = 0; state < 4; ++state) for (dropMode = 0; dropMode < 3; ++dropMode)
    for (bodyMode = 0; bodyMode < 3; ++bodyMode) for (finalMode = 0; finalMode < 3; ++finalMode) for (ambientMode = 0; ambientMode < 3; ++ambientMode)
        run_case(state);
    assert(executions == observations);
    if (!smoke) {
        unsigned baseline[2][3], shape, fault;
        for (shape = 0; shape < 2; ++shape) for (dropMode = 0; dropMode < 3; ++dropMode) {
            kind = shape ? 5 : 2; ambientMode = 0; baseline[shape][dropMode] = fault_case(-1);
            assert(baseline[shape][dropMode] >= 4 && baseline[shape][dropMode] < 12);
            printf("async source lease allocation baseline: kind=%d drop=%d allocations=%u\n", kind, dropMode, baseline[shape][dropMode]);
        }
        /* Sweep every actual allocation, including final boxing. A raising
         * source destructor can terminate before return-copy/boxing; measure
         * that path independently and require every armed fault to trigger. */
        for (round = 0; round < 10; ++round) for (shape = 0; shape < 2; ++shape)
        for (dropMode = 0; dropMode < 3; ++dropMode) for (fault = 0; fault < baseline[shape][dropMode]; ++fault)
        for (ambientMode = 0; ambientMode < 3; ++ambientMode) { kind = shape ? 5 : 2; fault_case((int)fault); }
        printf("async source lease: %u real launch/capture allocation failures, original MemoryError protected from raising source cleanup\n", faults);
        {
            unsigned present, entryBaseline[2], start = faults;
            ambientMode = 0;
            for (present = 0; present < 2; ++present) {
                entryBaseline[present] = entry_fault_case((int)present, -1);
                assert(entryBaseline[present] == 2 + present);
            }
            for (round = 0; round < 20; ++round) for (present = 0; present < 2; ++present)
            for (fault = 0; fault < entryBaseline[present]; ++fault) for (ambientMode = 0; ambientMode < 3; ++ambientMode)
                entry_fault_case((int)present, (int)fault);
            printf("parameter acquisition: %u real copy failures; nullable null success, exact installed prefix rollback, later BYVAL owner retired, caller borrows preserved\n", faults-start);
        }
    }
    source_lease_finish(); xrtErrorFree(inputError);
    if (!smoke) { balanced(&before); xrtMemDebugSnapshot(&after);
        printf("async source lease: %u executions/publication observers, %llu balanced allocations; unique/shared ready and pending, four input outcomes, raising payload/body/finally, three ambient states\n",
            executions, (unsigned long long)(after.AllocCount-before.AllocCount));
    } else printf("async source lease C-host smoke: %u executions\n", executions);
    return 0;
}
