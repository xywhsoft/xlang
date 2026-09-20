#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include "xlir_code_finalizer_drop.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>
typedef struct Owner Owner;
typedef struct Context {
    Owner* owner;
    unsigned id, calls, releases, fields;
    xvalue* owned;
    FinalizerHooks hooks;
} Context;
struct Owner {
    HMODULE library;
    void* address;
    unsigned acquired, released, pins, unloaded, mode;
    bool closed;
    xvalueiter deferred;
    Context contexts[2];
    xerror *hostError, *callbackError;
};
static unsigned allocationFailures, refusedBindings, probes, inspectionFailures;
static void mapped(const Owner* owner)
{
    MEMORY_BASIC_INFORMATION memory;
    assert(!owner->unloaded && owner->pins == 1 && owner->closed);
    assert(VirtualQuery(owner->address, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_COMMIT);
}
static int acquire(void* data)
{
    Owner* owner = (Owner*)data;
    if (owner->closed) return 0;
    ++owner->pins; ++owner->acquired; return 1;
}
static void release(void* data)
{
    Owner* owner = (Owner*)data;
    assert(owner->pins); --owner->pins; ++owner->released;
    if (owner->closed && !owner->pins) {
        MEMORY_BASIC_INFORMATION memory;
        for (unsigned i = 0; i < 2; ++i) assert(owner->contexts[i].calls == 1 && owner->contexts[i].releases == 1 &&
            owner->contexts[i].hooks.finalizedReturns == 1 && owner->contexts[i].hooks.releasedReturns == 1);
        assert(owner->contexts[1].fields == 1 && owner->contexts[1].hooks.fieldReturns == 1);
        assert(FreeLibrary(owner->library));
        assert(VirtualQuery(owner->address, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_FREE);
        ++owner->unloaded;
    }
    xrtSetErrorTake(xrtErrorRef(owner->hostError));
}
static bool owner_count(const void* data, size_t* result)
{
    const Owner* owner = (const Owner*)data;
    if (owner->unloaded) return false;
    *result = owner->pins + !owner->closed; return true;
}
static bool owner_trace(const void* data, xrtownershipvisitor visit, ptr context)
{ (void)data; (void)visit; (void)context; return true; }
static const xrtownershipops owner_ops = {owner_count, owner_trace};
static bool leaf(const xvalue* value, xrtownershipvisitor visit, ptr context)
{ (void)value; (void)visit; (void)context; return true; }
static bool trace(const void* data, XL6_ModuleOwnershipVisit visit, void* context)
{
    const Context* payload = (const Context*)data;
    mapped(payload->owner);
    return payload->owned == NULL || visit(xrtValueOwnership(payload->owned), context);
}
static void finalize(xvalue* value, void* data)
{
    Context* payload = (Context*)data;
    mapped(payload->owner); assert(!payload->calls && !payload->releases);
    assert(xrtValueCount(value) == 1); ++payload->calls;
    if (payload->id == 0 && payload->owner->mode == 6)
        assert(xrtValueIterBegin(value, &payload->owner->deferred));
}
static void end_context(void* data)
{
    Context* payload = (Context*)data;
    mapped(payload->owner);
    assert(payload->calls == 1 && payload->hooks.finalizedReturns == 1 && !payload->releases);
    if (payload->id == 1) assert(payload->fields == 1 && payload->hooks.fieldReturns == 1);
    else if (payload->owner->mode != 5) assert(payload->owner->contexts[1].releases == 1);
    ++payload->releases; xrtValueRelease(payload->owned); payload->owned = NULL;
    xrtSetErrorTake(xrtErrorRef(payload->owner->callbackError));
}
static void field(void* data)
{
    Context* payload = (Context*)data;
    mapped(payload->owner);
    assert(payload->id == 1 && payload->calls == 1 && payload->hooks.finalizedReturns == 1 && !payload->releases && !payload->fields);
    ++payload->fields;
}
static void graph(Owner* owner, xrtownershipref root, size_t nodes, size_t edges)
{
    xrtownershipref anchors[2] = {{owner, &owner_ops}, root};
    xrtownershipresult initial = {91,92,93,94}, result = initial;
    bool ok = xrtOwnershipInspect(anchors, 2, NULL, 0, &result);
    if (owner->mode == 9) { assert(!ok && !memcmp(&initial, &result, sizeof(result))); xrtClearError(); return; }
    assert(ok && result.NodeCount == nodes && result.EdgeCount == edges && result.ExternalRootCount == 1 && result.ReachableAnchorCount == 2);
    for (unsigned fault = 0; fault < 14; ++fault) {
        bool triggered;
        result = initial; assert(xrtMemDebugFailAfter(fault));
        ok = xrtOwnershipInspect(anchors, 2, NULL, 0, &result);
        triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); ++probes;
        if (triggered) { ++inspectionFailures; assert(!ok && !memcmp(&initial, &result, sizeof(result))); }
        else assert(ok && result.NodeCount == nodes && result.EdgeCount == edges && result.ExternalRootCount == 1);
        xrtClearError();
    }
}
int main(int argc, char** argv)
{
    assert(argc == 2 && xrtMemDebugEnable(true));
    for (unsigned round = 0; round < 25; ++round) for (unsigned mode = 0; mode < 10; ++mode) {
        Owner owner = {0}; xmemdebugsnapshot before, after;
        XL6_CodeOwnerGraph descriptor = {{sizeof(descriptor), &owner, acquire, release}, 0x584C36434F574E31ULL, &owner_ops};
        XL6_CodeLease* parent = NULL;
        XL6_CodeFinalizeProc finalizer; XL6_CodeContextTrace tracer; XL6_CodeContextRelease releaser;
        xvaluehandledrop fieldDrop; xvaluehandleops fieldOps = {0};
        xvalue *object, *child, *handle, *extra = NULL; ptr native;
        xvalueiter stack = {0}, *heap = NULL; FARPROC symbol;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        owner.mode = mode; owner.library = LoadLibraryA(argv[1]); assert(owner.library);
        symbol = GetProcAddress(owner.library, "finalizer_generated_finalize"); memcpy(&finalizer, &symbol, sizeof(finalizer)); owner.address = (void*)symbol;
        symbol = GetProcAddress(owner.library, "finalizer_generated_trace"); memcpy(&tracer, &symbol, sizeof(tracer));
        symbol = GetProcAddress(owner.library, "finalizer_generated_release"); memcpy(&releaser, &symbol, sizeof(releaser));
        symbol = GetProcAddress(owner.library, "finalizer_generated_field"); memcpy(&fieldDrop, &symbol, sizeof(fieldDrop));
        assert(finalizer && tracer && releaser && fieldDrop);
        owner.hostError = xrtErrorCreate(XERR_STATE, "host", 31, "release diagnostic");
        owner.callbackError = xrtErrorCreate(XERR_STATE, "callback", 32, "context diagnostic"); assert(owner.hostError && owner.callbackError);
        if (mode == 9) descriptor.base.size = sizeof(XL6_CodeOwner);
        assert(xl6CodeLeaseAcquire(&descriptor.base, &parent) == XL6_CODE_LEASE_ACQUIRED && parent);
        owner.closed = true; /* Children inherit the accepted capability after Close. */
        for (unsigned i = 0; i < 2; ++i) {
            Context* context = &owner.contexts[i]; context->owner = &owner; context->id = i;
            context->owned = xrtValueInt(40 + i); assert(context->owned);
            context->hooks = (FinalizerHooks){context, finalize, trace, end_context, field, 0, 0, 0};
        }
        object = xrtValueObjectLifo(); child = xrtValueObjectLifo(); assert(object && child);
        native = &owner.contexts[1].hooks; fieldOps.Drop = fieldDrop;
        handle = xrtValueHandleTake(&native, &fieldOps, NULL);
        assert(handle && xrtValueHandleOwnershipBind(handle, leaf) && xrtValueObjectSetNew(child, XRT_STR_LITERAL("native"), handle));
        assert(xl6CodeFinalizerBind(child, parent, finalizer, &owner.contexts[1].hooks, tracer, releaser));
        assert(xrtValueObjectSetNew(object, XRT_STR_LITERAL("child"), child));
        if (mode == 7) {
            xerror* prior = round & 1 ? xrtErrorRef(owner.callbackError) : NULL;
            xrtSetErrorTake(prior); assert(xrtMemDebugFailAfter(0));
            assert(!xl6CodeFinalizerBind(object, parent, finalizer, &owner.contexts[0].hooks, tracer, releaser));
            assert(xrtMemDebugFailTriggered()); ++allocationFailures; xrtMemDebugFailClear();
            assert(prior ? xrtGetError() == prior : xrtErrorKind(xrtGetError()) == XERR_MEMORY); xrtClearError();
        }
        if (mode == 8) {
            extra = xrtValueRetain(object); xrtSetErrorTake(xrtErrorRef(owner.callbackError));
            assert(!xl6CodeFinalizerBind(object, parent, finalizer, &owner.contexts[0].hooks, tracer, releaser));
            assert(xrtGetError() == owner.callbackError && owner.released == 0 && !owner.contexts[0].releases);
            ++refusedBindings; xrtClearError(); xrtValueRelease(extra); extra = NULL;
        }
        assert(xl6CodeFinalizerBind(object, parent, finalizer, &owner.contexts[0].hooks, tracer, releaser));
        xl6CodeLeaseRelease(parent); assert(owner.acquired == 1 && owner.released == 0);
        graph(&owner, xrtValueOwnership(object), 9, 9);
        if (mode == 1) extra = xrtValueRetain(object);
        if (mode == 2) extra = xrtValueClone(object);
        if (mode == 3) assert(xrtValueIterBegin(object, &stack));
        if (mode == 4) { heap = xrtValueIterRCreate(object); assert(heap); }
        if (mode == 5) extra = xrtValueRetain(child);
        xrtValueRelease(object);
        if (mode >= 1 && mode <= 5) {
            mapped(&owner); xrtClearError();
            if (mode <= 2) graph(&owner, xrtValueOwnership(extra), 9, 9);
            if (mode == 3 || mode == 4) graph(&owner, xrtValueIterOwnership(mode == 3 ? &stack : heap), 10, 11);
            if (mode == 5) graph(&owner, xrtValueOwnership(extra), 6, 5);
        }
        if (mode == 6) { mapped(&owner); assert(!owner.contexts[0].releases && !owner.contexts[1].calls); xrtValueIterEnd(&owner.deferred); }
        xrtValueRelease(extra); xrtValueIterEnd(&stack); xrtValueIterDestroy(heap);
        assert(owner.unloaded == 1 && owner.acquired == 1 && owner.released == 1 && !owner.pins);
        assert(xrtGetError() == owner.callbackError); xrtClearError(); xrtErrorFree(owner.callbackError); xrtErrorFree(owner.hostError);
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    assert(allocationFailures == 25 && refusedBindings == 25 && probes == 4900 && inspectionFailures == 1400);
    printf("Resident object finalizer: 250 real DLL lifetimes, 500 finalizers/contexts, 250 field drops, %u allocation failures, %u publication refusals, %u graph probes, %u actual inspection failures\n",
        allocationFailures, refusedBindings, probes, inspectionFailures);
    assert(xrtMemDebugReset()); return 0;
}
