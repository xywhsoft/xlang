#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include <assert.h>
#include <stdio.h>

static void* create(int family)
{
    switch (family) {
    case 0: return xl6AtomicInt32CreateV1(0);
    case 1: return xl6AtomicUInt32CreateV1(0);
    case 2: return xl6AtomicInt64CreateV1(0);
    case 3: return xl6AtomicUInt64CreateV1(0);
    case 4: return xl6AtomicBoolCreateV1(false);
    default: assert(0); return NULL;
    }
}

static const XL6_NativeHandlePolicyV1* policy(int family)
{
    switch (family) {
    case 0: return xl6AtomicInt32HandlePolicyV1Get();
    case 1: return xl6AtomicUInt32HandlePolicyV1Get();
    case 2: return xl6AtomicInt64HandlePolicyV1Get();
    case 3: return xl6AtomicUInt64HandlePolicyV1Get();
    case 4: return xl6AtomicBoolHandlePolicyV1Get();
    default: assert(0); return NULL;
    }
}

static void fault(int family, size_t point)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(point));
    void* raw = create(family);
    xvalue* boxed = raw ? xl6NativeHandleTakeV1(&raw, policy(family)) : NULL;
    bool hit = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    assert(!raw);
    if (hit) assert(!boxed && xrtGetError());
    else assert(boxed);
    xerror* error = xrtTakeError();
    xrtValueRelease(boxed);
    xrtErrorFree(error);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
}

int main(void)
{
    assert(xrtMemDebugEnable(true));
    for (int family = 0; family != 5; ++family) {
        void* raw = create(family);
        assert(raw);
        xvalue* boxed = xl6NativeHandleTakeV1(&raw, policy(family));
        assert(boxed && !raw);
        xrtValueRelease(boxed);
    }
    xrtClearError();
    for (int family = 0; family != 5; ++family)
        for (size_t point = 0; point != 8; ++point) fault(family, point);
    puts("atomic 40 allocation prefixes balanced");
    return 0;
}
