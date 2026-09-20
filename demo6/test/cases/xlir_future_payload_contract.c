#define XRT_MODULE_ALL
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_future_payload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void observe_outcomes(bool smoke)
{
    for (unsigned round = 0; round < 100; ++round) {
        xmemdebugsnapshot before, after; xerror* source;
        if (!smoke) xrtMemDebugSnapshot(&before);
        source = xrtErrorCreate(XERR_PROTOCOL, "observe", 42, "original"); assert(source);
        assert(!xl6PayloadObserveOutcome(XFUTURE_FAILED, source) && xrtGetError() == source);
        xrtErrorFree(source); assert(xrtErrorCode(xrtGetError()) == 42); xrtClearError();
        assert(!xl6PayloadObserveOutcome(XFUTURE_FAILED, NULL) && xrtErrorKind(xrtGetError()) == XERR_INTERNAL); xrtClearError();
        assert(!xl6PayloadObserveOutcome(XFUTURE_CANCELLED, NULL) && xrtErrorKind(xrtGetError()) == XERR_CANCELLED);
        assert(strcmp(xrtErrorMessage(xrtGetError()), "future cancel requested.") == 0);
        assert(strcmp(xrtErrorData(xrtGetError()), "CancelledError") == 0); xrtClearError();
        assert(!xl6PayloadObserveOutcome(XFUTURE_CLOSED, NULL) && xrtErrorKind(xrtGetError()) == XERR_CLOSED); xrtClearError();
        assert(!xl6PayloadObserveOutcome(-1, NULL) && xrtErrorKind(xrtGetError()) == XERR_STATE);
        assert(xl6PayloadObserveOutcome(XFUTURE_RESOLVED, NULL) && !xrtGetError());
        if (!smoke) {
            assert(xrtMemDebugFailAfter(0));
            assert(!xl6PayloadObserveOutcome(XFUTURE_CANCELLED, NULL) && xrtMemDebugFailTriggered());
            xrtMemDebugFailClear(); assert(xrtErrorKind(xrtGetError()) == XERR_CANCELLED); xrtClearError();
            xrtMemDebugSnapshot(&after);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
            assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        }
    }
    puts("Future observation: 600 outcomes, original error identity and cancellation allocation fallback");
}
static void* make_payload(unsigned kind, bool empty)
{
    void* box = xrtMalloc(kind == 0 ? sizeof(int64_t) : sizeof(void*));
    assert(box);
    if (kind == 0) *(int64_t*)box = 42;
    else if (kind == 1) { *(char**)box = empty ? NULL : xrtStrDup("resident"); assert(empty || *(char**)box); }
    else if (kind == 2) { *(xvalue**)box = empty ? NULL : xrtValueInt(42000); assert(empty || *(xvalue**)box); }
    else if (kind == 3) { *(xerror**)box = empty ? NULL : xrtErrorCreate(XERR_STATE, "payload", 71, "data"); assert(empty || *(xerror**)box); }
    else {
        xfuture* inner = NULL;
        if (!empty) {
            xpromise* promise = xrtPromiseCreate(&inner, NULL); assert(promise);
            void* nested = make_payload(1, false);
            assert(xl6PayloadResolveOwned(promise, nested, XL6_FUTURE_PAYLOAD_STRING));
            xrtPromiseDestroy(promise);
        }
        *(xfuture**)box = inner;
    }
    return box;
}
int main(int argc, char** argv)
{
    unsigned round, kind, lane;
    bool smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    XL6_FuturePayloadDrop drops[5];
    for (kind = 0; kind < 5; ++kind) {
        drops[kind] = xl6PayloadDropGet((XL6_FuturePayloadKind)(kind + 1)); assert(drops[kind]);
        assert(xl6PayloadTraceGet((XL6_FuturePayloadKind)(kind + 1)));
    }
    assert(!xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_INVALID));
    assert(!xl6PayloadDropGet((XL6_FuturePayloadKind)-1));
    assert(xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_CLOSURE));
    assert(!xl6PayloadDropGet((XL6_FuturePayloadKind)7));
    assert(!xl6PayloadTraceGet(XL6_FUTURE_PAYLOAD_INVALID));
    assert(!xl6PayloadTraceGet((XL6_FuturePayloadKind)-1));
    assert(xl6PayloadTraceGet(XL6_FUTURE_PAYLOAD_CLOSURE));
    assert(!xl6PayloadTraceGet((XL6_FuturePayloadKind)7));
    if (!smoke) assert(xrtMemDebugEnable(true));
    observe_outcomes(smoke);
    for (round = 0; round < 100; ++round) for (kind = 0; kind < 5; ++kind) for (lane = 0; lane < 3; ++lane) {
        xmemdebugsnapshot before, after;
        xfuture* future = NULL;
        xpromise* promise;
        void* box;
        xrtClearError();
        if (!smoke) xrtMemDebugSnapshot(&before);
        drops[kind](NULL, NULL);
        box = make_payload(kind, lane == 2);
        promise = xrtPromiseCreate(&future, NULL); assert(promise && future);
        if (lane == 1) {
            assert(xrtPromiseResolve(promise, NULL));
            assert(!xl6PayloadResolveOwned(promise, box, (XL6_FuturePayloadKind)(kind + 1)));
            xrtClearError(); drops[kind](box, NULL); /* Publication did not consume. */
        } else {
            assert(!xl6PayloadResolveOwned(promise, box, XL6_FUTURE_PAYLOAD_INVALID));
            assert(xrtFutureState(future) == XFUTURE_PENDING); xrtClearError();
            assert(xl6PayloadResolveOwned(promise, box, (XL6_FuturePayloadKind)(kind + 1)));
            assert(xrtFutureValue(future) == box);
            {
                xrtownershipref slots[2] = {xrtFutureOwnership(future), xrtPromiseOwnership(promise)};
                xrtownershipresult graph = {0}; bool live = true;
                assert(xrtOwnershipInspectReachable(slots, 1, slots, 2, &live, &graph, NULL, NULL));
                assert(!live && graph.ExternalRootCount == 0);
                assert(xrtFutureRef(future) == future);
                assert(xrtOwnershipInspectReachable(slots, 1, slots, 2, &live, &graph, NULL, NULL));
                assert(live && graph.ExternalRootCount == 1); xrtFutureDestroy(future);
            }
            if (kind == 4 && lane == 0) {
                /* Unbox/retain survives outer box destruction, no flattening. */
                xfuture* inner = xrtFutureRef(*(xfuture**)box); assert(inner);
                xrtPromiseDestroy(promise); promise = NULL;
                xrtFutureDestroy(future); future = NULL;
                assert(strcmp(*(char**)xrtFutureValue(inner), "resident") == 0);
                xrtFutureDestroy(inner);
            }
        }
        xrtPromiseDestroy(promise); xrtFutureDestroy(future); xrtClearError();
        if (!smoke) {
            xrtMemDebugSnapshot(&after);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
            assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        }
    }
    puts(smoke ? "Future payload C-host: 1500 accepted/rejected/empty cases, nested retain, no allocator balance claim" :
        "Future payload: 1500 accepted/rejected/empty cases, nested retain, exact memory balance");
    return 0;
}
