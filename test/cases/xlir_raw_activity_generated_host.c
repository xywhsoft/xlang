#include XL6_RAW_ACTIVITY_GENERATED_C
#include "xlir_raw_activity_vectors.h"
typedef struct StaticOwner { unsigned pins, acquisitions, releases; bool closed; } StaticOwner;
static StaticOwner staticOwner;
static int acquire(void* data)
{
    StaticOwner* owner = data; if (owner->closed) return 0;
    ++owner->pins; ++owner->acquisitions; return 1;
}
static void release(void* data)
{ StaticOwner* owner = data; assert(owner->pins); --owner->pins; ++owner->releases; }
static void inspectActivity(bool active, bool foreign)
{
    xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6CodeOwnerScopeActive(trackedOwner) == active && xl6CodeOwnerScopeForeign(trackedOwner) == foreign);
    assert(xrtOwnershipScopeEnd(&freeze));
}
int main(void)
{
    XL6_CodeOwner owner = {sizeof(owner), &staticOwner, acquire, release}; trackedOwner = &owner;
    xl6ProductMemoryBegin(SIZE_MAX); xmemdebugsnapshot baseline; xrtMemDebugSnapshot(&baseline);
    assert(xl_script_bind_code_owner(&owner));
    xl6_foreign_error_scope call = {0}; assert(xl6ForeignCallBegin(&call)); capture_raw_activity();
    xl6_foreign_error_result result = xl6ForeignCallEnd(&call); assert(result.ok && !result.error && !depth);
    unsigned acquisitions = staticOwner.acquisitions; observing = true; inspectActivity(false, false);
    unsigned vectors = runVectors(); size_t attempts = runOom(); missingDiagnosticOom(); directOom(); foreignCallback();
    staticOwner.closed = true; directOom(); observing = false;
    assert(staticOwner.acquisitions == acquisitions); xlir_module_fini(); assert(!xrtGetError());
    assert(!staticOwner.pins && staticOwner.acquisitions == staticOwner.releases);
    assert(xrtAtomic64Load(&enters, XMEMORY_RELAXED) == xrtAtomic64Load(&leaves, XMEMORY_RELAXED));
    xrtClearError(); xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == baseline.LiveCount && after.LiveBytes == baseline.LiveBytes);
    assert(after.AllocCount - baseline.AllocCount == after.FreeCount - baseline.FreeCount);
    assert(!after.DoubleFreeCount && !after.InvalidFreeCount && !after.UseAfterFreeCount);
    printf("generated C raw activity: %u saved-callback vectors, %zu closed OOM attempts, foreign callback, no new capabilities and balanced heap\n", vectors, attempts);
    return 0;
}
