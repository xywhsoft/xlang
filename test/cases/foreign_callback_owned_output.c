#include "foreign_callback_owned_output.h"
#include <xrt_decl.h>
#include <assert.h>

struct CallbackOwnedOutput { int tag; };
static unsigned live, dropOrder;

int xl_test_owned_output_store(CallbackOwnedOutput** slot, int tag)
{
    if (!slot || *slot) {
        xrtSetErrorKind(XERR_STATE);
        return -9;
    }
    if (tag == -1) {
        if ((!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) || !xrtMemDebugFailAfter(0)) {
            return -10;
        }
    }
    CallbackOwnedOutput* value = xrtMalloc(sizeof(*value));
    if (tag == -1) {
        assert(!value && xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
    }
    if (!value) return -8;
    value->tag = tag;
    *slot = value;
    ++live;
    return 0;
}

void xl_test_owned_output_drop(CallbackOwnedOutput* value)
{
    assert(value && live);
    int tag = value->tag;
    dropOrder = dropOrder * 10 + (unsigned)(tag % 10);
    --live;
    xrtFree(value);
    if (tag == 12) xrtSetErrorKind(XERR_STATE);
}

int xl_test_owned_output_invoke(CallbackOwnedOutputProc handler, void* user, int mode)
{
    static const int expectedStatuses[] = {0, -7, -16, -16, -16, -16, -16, -16, -7};
    static const unsigned expectedDropOrder[] = {12, 21, 1, 1, 21, 0, 1, 1, 21};
    assert(mode >= 0 && (size_t)mode < sizeof(expectedStatuses) / sizeof(*expectedStatuses));
    assert(!live);
    dropOrder = 0;

    CallbackOwnedOutput sentinel = {0};
    CallbackOwnedOutput* first = &sentinel;
    CallbackOwnedOutput* second = &sentinel;
    int status = handler(mode == 5 ? NULL : &first, &second, mode, user);
    assert(status == expectedStatuses[mode]);
    if (status == 0) {
        assert(first && second && first->tag == 1 && second->tag == 2 && live == 2);
        xl_test_owned_output_drop(first);
        xl_test_owned_output_drop(second);
    } else {
        assert(first == (mode == 5 ? &sentinel : NULL) && !second);
    }
    assert(!live && dropOrder == expectedDropOrder[mode]);
    return status;
}

static int dispatchRetained(CallbackOwnedOutput** first, CallbackOwnedOutput** second, int mode, void* user)
{
    const CallbackOutputBackend* backend = user;
    return backend->write(backend, first, second, mode);
}

int xl_test_owned_output_retained(const CallbackOutputBackend* backend, const CallbackOutputOwner* owner, int mode)
{
    if (!owner || !owner->Hold(backend->user)) return -11;
    CallbackOutputBackend snapshot = *backend;
    int status = xl_test_owned_output_invoke(dispatchRetained, &snapshot, mode);
    if (!owner->Drop(snapshot.user) && status == 0) return -11;
    return status;
}

static int nativeWrite(CallbackOwnedOutput** first, CallbackOwnedOutput** second, int mode, void* user)
{
    (void)mode;
    (void)user;
    *first = *second = NULL;
    int status = xl_test_owned_output_store(first, 1);
    return status ? status : xl_test_owned_output_store(second, 2);
}

int xl_test_owned_output_warm(CallbackOwnedOutputProc handler, void* user)
{
    const unsigned rounds = 100;
    if (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return -10;
    if (xl_test_owned_output_invoke(handler, user, 0)) return -11;
    xmemdebugsnapshot before, baseline, after;
    xrtMemDebugSnapshot(&before);
    for (unsigned index = 0; index < rounds; ++index) {
        if (xl_test_owned_output_invoke(nativeWrite, NULL, 0)) return -11;
    }
    xrtMemDebugSnapshot(&baseline);
    for (unsigned index = 0; index < rounds; ++index) {
        if (xl_test_owned_output_invoke(handler, user, 0)) return -11;
    }
    xrtMemDebugSnapshot(&after);
    assert(baseline.AllocCount - before.AllocCount == 2 * rounds);
    assert(baseline.FreeCount - before.FreeCount == 2 * rounds);
    assert(after.AllocCount - baseline.AllocCount == 2 * rounds);
    assert(after.FreeCount - baseline.FreeCount == 2 * rounds);
    assert(after.ReallocCount == before.ReallocCount && after.LiveCount == before.LiveCount);
    assert(after.LiveBytes == before.LiveBytes && after.DoubleFreeCount == before.DoubleFreeCount);
    assert(after.InvalidFreeCount == before.InvalidFreeCount);
    return 0;
}
