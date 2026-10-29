/* Capability probe, NOT the language object-escape acceptance gate.
 * All callbacks/descriptors are permanently resident in this executable.
 * Collection is single-threaded at a quiescent point. Never copy this test's
 * deliberately incomplete shared-Value trace adapter into production. */
#define XRUNTIME_MODULE_RUNTIME_VALUE_ROOTS
#define XRT_MODULE_MEMORY_DEBUG
#ifdef XLIR_GRAPH_SNAPSHOT
#define XRUNTIME_IMPLEMENTATION
#endif
#include "lib/xruntime/xruntime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Node {
    xrtobject* next;
    xvalue* slot;
} Node;
static unsigned dropped;
static bool node_init(ptr data, const xrttype* type)
{
    (void)type; memset(data, 0, sizeof(Node)); return true;
}
static void node_drop(ptr data, const xrttype* type)
{
    Node* node = (Node*)data;
    xrtobject* next = node->next;
    xvalue* slot = node->slot;
    (void)type;
    node->next = NULL; node->slot = NULL; ++dropped;
    xrtObjectUnref(next); xrtValueRelease(slot);
}
static bool node_trace(const void* data, const xrttype* type,
                       xrtobjectvisitor visit, ptr context)
{
    const Node* node = (const Node*)data;
    (void)type;
    if (node->next && !visit(node->next, context)) return false;
    /* A plausible but INCOMPLETE adapter: it cannot identify external aliases
     * of slot or backing, and independent invocations do not share a seen set. */
    return !node->slot || xrtValueTraceRuntimeObjects(node->slot, visit, context);
}
static const xrtinstanceops node_ops = {node_init, node_drop, node_trace};
static xrttype node_type;
static Node* data(xrtobject* object) { Node* node = xrtObjectData(object); assert(node); return node; }

typedef struct Cycle {
    xrtobjectgraph* graph;
    xrtobject* module; /* borrowed after construction */
    xrtobject* object; /* borrowed after construction */
    xrtweak weak;
} Cycle;
static Cycle make_cycle(bool array)
{
    Cycle cycle = {0};
    xvalue* value;
    dropped = 0;
    cycle.graph = xrtObjectGraphCreate(); assert(cycle.graph);
    cycle.module = xrtObjectCreate(&node_type);
    cycle.object = xrtObjectCreate(&node_type);
    assert(cycle.module && cycle.object);
    assert(xrtObjectGraphTrack(cycle.graph, cycle.module));
    assert(xrtObjectGraphTrack(cycle.graph, cycle.object));
    assert(xrtWeakInit(&cycle.weak, cycle.object));
    data(cycle.object)->next = xrtObjectRef(cycle.module);
    value = xrtValueRuntimeObject(cycle.object); assert(value);
    if (array) {
        data(cycle.module)->slot = xrtValueArray(); assert(data(cycle.module)->slot);
        assert(xrtValueArrayAppendTake(data(cycle.module)->slot, &value) && !value);
    } else data(cycle.module)->slot = value;
    xrtObjectUnref(cycle.object); xrtObjectUnref(cycle.module);
    assert(xrtObjectRefCount(cycle.object) == 1);
    return cycle;
}
static void collect(Cycle* cycle, xvalue* root, size_t expected)
{
    xrtobjectgraphresult result = {0};
    assert(root ? xrtObjectGraphCollectValueRoot(cycle->graph, root, &result) :
                  xrtObjectGraphCollect(cycle->graph, &result));
    assert(result.TrackedCount == 2 && result.EdgeCount == 2);
    assert(result.CollectedCount == expected && dropped == expected);
    assert(xrtObjectGraphCount(cycle->graph) == 2 - expected);
    assert(xrtWeakExpired(&cycle->weak) == (expected == 2));
    assert(!xrtGetError());
}
static void finish(Cycle* cycle)
{
    assert(dropped == 2 && xrtObjectGraphCount(cycle->graph) == 0);
    xrtWeakUnit(&cycle->weak); xrtObjectGraphDestroy(cycle->graph);
}
static void boundary_case(unsigned mode)
{
    bool array = mode >= 5;
    Cycle cycle = make_cycle(array);
    xvalue* host = NULL;
    xrtobject* raw = NULL;
    if (mode == 1) raw = xrtObjectRef(cycle.object);
    if (mode == 2) host = xrtValueRuntimeObject(cycle.object); /* independent owning slot */
    if (mode == 3 || mode == 4) {
        host = xrtValueRetain(data(cycle.module)->slot);
        assert(host == data(cycle.module)->slot);
    }
    if (mode == 5 || mode == 6) {
        host = xrtValueClone(data(cycle.module)->slot);
        assert(host && host != data(cycle.module)->slot); /* COW shells share backing */
    }
    if (mode == 7) host = xrtValueRetain(xrtValueArrayGet(data(cycle.module)->slot, 0));
    assert(mode == 0 || mode == 1 || host);
    assert(xrtObjectRefCount(cycle.object) == (mode == 1 || mode == 2 ? 2u : 1u));
    if (mode == 0 || mode == 3 || mode == 5) {
        /* Modes 3/5 are counterexamples to automatic-root inference from a
         * flattened Value trace, NOT desirable language semantics. Check only
         * weak liveness after collection; never dereference finalized payload. */
        collect(&cycle, NULL, 2);
        xrtValueRelease(host);
    } else {
        collect(&cycle, mode >= 4 ? host : NULL, 0);
        assert(data(cycle.object)->next == cycle.module);
        xrtValueRelease(host); xrtObjectUnref(raw);
        collect(&cycle, NULL, 2);
    }
    finish(&cycle);
}
static void duplicated_trace_case(void)
{
    Cycle cycle = make_cycle(false);
    xrtobjectgraphresult result = {91,92,93,94};
    /* Two graph nodes share ONE wrapper-owned object reference. Each separate
     * trace reports it once; summing those reports overcounts actual edges. */
    data(cycle.object)->slot = xrtValueRetain(data(cycle.module)->slot);
    assert(!xrtObjectGraphCollect(cycle.graph, &result));
    assert(xrtGetError() && dropped == 0 && !xrtWeakExpired(&cycle.weak));
    assert(result.TrackedCount == 91 && result.EdgeCount == 92 &&
           result.RootCount == 93 && result.CollectedCount == 94);
    assert(xrtObjectRefCount(cycle.object) == 1 && xrtObjectGraphCount(cycle.graph) == 2);
    xrtClearError();
    xrtValueRelease(data(cycle.object)->slot); data(cycle.object)->slot = NULL;
    collect(&cycle, NULL, 2); finish(&cycle);
}
static void allocation_case(unsigned index)
{
    Cycle cycle = make_cycle(false);
    xrtobjectgraphresult result = {91,92,93,94};
    assert(xrtMemDebugFailAfter(index));
    assert(!xrtObjectGraphCollect(cycle.graph, &result));
    assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xrtGetError() && dropped == 0 && !xrtWeakExpired(&cycle.weak));
    assert(result.TrackedCount == 91 && result.EdgeCount == 92 &&
           result.RootCount == 93 && result.CollectedCount == 94);
    assert(xrtObjectRefCount(cycle.object) == 1 && xrtObjectGraphCount(cycle.graph) == 2);
    xrtClearError(); collect(&cycle, NULL, 2); finish(&cycle);
}
static void check_balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
}
int main(void)
{
    unsigned round, mode;
    node_type.Kind = XRT_TYPE_CLASS;
    node_type.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_COPYABLE;
    node_type.Name = (xstrview){"XLIR graph probe", 16};
    node_type.AbiName = (xstrview){"xlir.graph.probe.v1", 19};
    node_type.Id = xrtTypeId(node_type.AbiName);
    node_type.Size = sizeof(xrtobject*); node_type.Align = _Alignof(xrtobject*);
    node_type.InstanceSize = sizeof(Node); node_type.InstanceAlign = _Alignof(Node);
    node_type.Ops = xrtObjectValueOps(); node_type.InstanceOps = &node_ops;
    assert(xrtTypeValidate(&node_type)); assert(xrtMemDebugEnable(true));
    for (round = 0; round < 100; ++round) {
        for (mode = 0; mode < 12; ++mode) {
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            if (mode < 8) boundary_case(mode);
            else if (mode == 8) duplicated_trace_case();
            else allocation_case(mode - 9);
            check_balance(&before);
        }
    }
    puts("Object graph boundary: 800 root/alias cases, 100 shared-edge rejection cases, 300 allocation faults; exact XRT memory balance.");
    puts("CAPABILITY: direct strong refs and explicitly enumerated Value roots preserve live cycles.");
    puts("COUNTEREXAMPLES: flattened shared Value/COW traces hide external roots; per-node shared-wrapper traces overcount edges.");
    puts("NOT ADMITTED: automatic XLIR object/module ownership, unregistered host aliases, concurrent collection, generated callback residency.");
    return 0;
}
