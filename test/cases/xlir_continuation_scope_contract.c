#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_MODULE_VALUE_CONTAINER
#include "lib/xrt/xrt.h"
#include "xlir_continuation_scope_dll.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

static const XL6_ClosureRuntimeV1* api;
typedef struct Fixture {
    XL6_CodeOwnerGraph owner;
    XL6_ContinuationQueue* queue;
    XL6_CodeLease* initialLease; /* Borrowed identity for equality checks only. */
    XL6_Closure* children[6]; /* Actual host references, not module globals. */
    bool closed, keepChildren;
    unsigned pins, acquired, dropped, denied, unmapped, length;
    HMODULE library; void* address;
    XL6_ContinuationProc run, release;
    ClosureTestInvoke invoke;
    ClosureTestDrop drop;
    XL6_CodeContextTrace trace;
    ClosureTestHooks closureHooks;
    ContinuationScopeHooks continuationHooks;
} Fixture;
static int acquire(void* data)
{
    Fixture* fixture = data;
    if (fixture->closed) { ++fixture->denied; return 0; }
    ++fixture->pins; ++fixture->acquired; return 1;
}
static void release_owner(void* data)
{
    Fixture* fixture = data;
    assert(fixture->pins); --fixture->pins; ++fixture->dropped;
    if (!fixture->pins && fixture->closed && fixture->library) {
        MEMORY_BASIC_INFORMATION memory;
        assert(fixture->continuationHooks.ran == fixture->length && fixture->continuationHooks.released == fixture->length);
        assert(fixture->closureHooks.dropped == (fixture->keepChildren ? fixture->length * 2 : 0));
        if (fixture->queue != NULL) {
            /* Last host drop still occupies the resident queue credit. It may
             * reenter Close/Destroy, but cannot destroy the executing queue. */
            assert(!xl6ContinuationQueueIsIdle(fixture->queue));
            assert(xl6ContinuationQueueActivationCount(fixture->queue) == 1);
            xl6ContinuationQueueClose(fixture->queue);
            assert(!xl6ContinuationQueueDestroy(fixture->queue));
        }
        assert(FreeLibrary(fixture->library)); fixture->library = NULL;
        assert(VirtualQuery(fixture->address, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_FREE);
        ++fixture->unmapped;
    }
}
static bool count(const void* data, size_t* result)
{ const Fixture* fixture = data; *result = fixture->pins + !fixture->closed; return true; }
static bool leaf(const void* data, xrtownershipvisitor visit, void* context)
{ (void)data; (void)context; return visit != NULL; }
static const xrtownershipops owner_ops = {count, leaf};
static void closure_during(ClosureTestEnvironment* environment) { (void)environment; }
static void closure_drop(ClosureTestEnvironment* environment) { xrtFree(environment); }
static bool closure_trace(const ClosureTestEnvironment* environment, xrtownershipvisitor visit, void* context)
{ (void)environment; (void)context; return visit != NULL; }
static void keep_child(ContinuationScopeFrame* frame, bool fromRelease)
{
    Fixture* fixture = frame->fixture;
    ClosureTestEnvironment* environment;
    unsigned index = frame->index * 2 + (fromRelease ? 1 : 0);
    if (!fixture->keepChildren) return;
    environment = xrtCalloc(1, sizeof(*environment)); assert(environment);
    environment->producer = &fixture->closureHooks;
    fixture->children[index] = api->Create(&fixture->owner.base, (void (*)(void))fixture->invoke, NULL,
        environment, fixture->drop, NULL, NULL, 0, fixture->trace);
    assert(fixture->children[index] && fixture->children[index]->lease == fixture->initialLease);
    assert(fixture->acquired == 1 && !fixture->dropped && fixture->closed);
}
static ContinuationScopeFrame* frame_new(Fixture* fixture, unsigned index)
{
    ContinuationScopeFrame* frame = xrtCalloc(1, sizeof(*frame)); assert(frame);
    frame->hooks = &fixture->continuationHooks; frame->fixture = fixture; frame->index = index; return frame;
}
static void run(void* data)
{
    ContinuationScopeFrame* frame = data; Fixture* fixture = frame->fixture;
    XL6_ContinuationTicket denied = {0};
    assert(fixture->continuationHooks.ran == frame->index && fixture->continuationHooks.released == frame->index);
    assert(fixture->closed && !fixture->unmapped && frame->ticket.codeLease == fixture->initialLease);
    /* An accepted callback may create lifetime children, not new roots. */
    assert(xl6ContinuationReserveOwned(fixture->queue, &denied, fixture->run, fixture->release,
        NULL, &fixture->owner.base) == XL6_CONTINUATION_CLOSED && denied.queue == NULL && !xrtGetError());
    keep_child(frame, false);
    if (frame->index + 1 < fixture->length) {
        ContinuationScopeFrame* next = frame_new(fixture, frame->index + 1);
        assert(xrtMemDebugFailAfter(0));
        assert(xl6ContinuationReserveSuccessor(&frame->ticket, &next->ticket,
            fixture->run, fixture->release, next) == XL6_CONTINUATION_ADMITTED);
        assert(next->ticket.codeLease == fixture->initialLease);
        assert(xl6ContinuationAbort(&next->ticket) && next->ticket.codeLease == NULL);
        assert(xl6ContinuationReserveSuccessor(&frame->ticket, &next->ticket,
            fixture->run, fixture->release, next) == XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationTraceBind(&next->ticket, leaf));
        xl6ContinuationReady(&next->ticket); xl6ContinuationCommit(&next->ticket);
        xl6ContinuationTransportReleased(&next->ticket);
        assert(!xl6ContinuationRunOne(fixture->queue));
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    }
}
static void release_frame(void* data)
{
    ContinuationScopeFrame* frame = data; Fixture* fixture = frame->fixture;
    assert(fixture->continuationHooks.ran == frame->index + 1 && fixture->continuationHooks.released == frame->index);
    assert(frame->ticket.queue == NULL && frame->ticket.ownershipRefs == 0);
    keep_child(frame, true); xrtFree(frame);
}
static void prepare(Fixture* fixture)
{
    *fixture = (Fixture){0};
    fixture->owner = (XL6_CodeOwnerGraph){{sizeof(fixture->owner), fixture, acquire, release_owner}, 0x584C36434F574E31ULL, &owner_ops};
    fixture->closureHooks = (ClosureTestHooks){closure_during, closure_drop, closure_trace, NULL, 0, 0, 0};
    fixture->continuationHooks = (ContinuationScopeHooks){run, release_frame, 0, 0};
}
static void symbol(HMODULE library, const char* name, void* output, size_t size)
{ FARPROC proc = GetProcAddress(library, name); assert(proc && size == sizeof(proc)); memcpy(output, &proc, size); }
static void load(Fixture* fixture, const char* path)
{
    fixture->library = LoadLibraryA(path); assert(fixture->library);
    symbol(fixture->library, "continuation_generated_run", &fixture->run, sizeof(fixture->run));
    symbol(fixture->library, "continuation_generated_release", &fixture->release, sizeof(fixture->release));
    symbol(fixture->library, "closure_generated_invoke", &fixture->invoke, sizeof(fixture->invoke));
    symbol(fixture->library, "closure_generated_drop", &fixture->drop, sizeof(fixture->drop));
    symbol(fixture->library, "closure_generated_trace", &fixture->trace, sizeof(fixture->trace));
    fixture->address = (void*)fixture->run;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount);
}
static DWORD WINAPI worker(void* data)
{ while (xl6ContinuationWaitRunOne(data)) {} return 0; }
static void graph_ticket(ContinuationScopeFrame* frame, size_t roots)
{
    xrtownershipref anchor = xl6ContinuationOwnership(&frame->ticket); xrtownershipresult result;
    assert(xrtOwnershipInspect(&anchor, 1, NULL, 0, &result));
    assert(result.NodeCount == 3 && result.EdgeCount == 2 && result.ExternalRootCount == roots && result.ReachableAnchorCount == 1);
}
static void graph_children(Fixture* fixture)
{
    xrtownershipref anchors[7]; size_t count = 0; xrtownershipresult result;
    for (unsigned i = 0; i < 6; ++i) if (fixture->children[i]) anchors[count++] = api->Ownership(fixture->children[i]);
    assert(count);
    anchors[count] = (xrtownershipref){fixture, &owner_ops};
    assert(xrtOwnershipInspect(anchors, count + 1, NULL, 0, &result));
    assert(result.NodeCount == count + 2 && result.EdgeCount == count + 1 &&
        result.ExternalRootCount == count && result.ReachableAnchorCount == count + 1);
}
static void lifetimes(const char* path)
{
    for (unsigned round = 0; round < 25; ++round) for (unsigned mode = 0; mode < 4; ++mode) {
        Fixture fixture; ContinuationScopeFrame* frame; xmemdebugsnapshot before;
        xrtMemDebugSnapshot(&before); prepare(&fixture); load(&fixture, path);
        fixture.length = mode < 2 ? 1 : 3; fixture.keepChildren = mode != 0;
        fixture.queue = xl6ContinuationQueueCreate(1); assert(fixture.queue);
        frame = frame_new(&fixture, 0);
        assert(xl6ContinuationReserveOwned(fixture.queue, &frame->ticket, fixture.run, fixture.release,
            frame, &fixture.owner.base) == XL6_CONTINUATION_ADMITTED);
        fixture.initialLease = frame->ticket.codeLease; assert(fixture.initialLease);
        assert(xl6ContinuationTraceBind(&frame->ticket, leaf)); xl6ContinuationCommit(&frame->ticket);
        graph_ticket(frame, 2); fixture.closed = true; xl6ContinuationQueueClose(fixture.queue); graph_ticket(frame, 1);
        xl6ContinuationReady(&frame->ticket); xl6ContinuationTransportReleased(&frame->ticket);
        if (mode == 3) {
            HANDLE threads[4];
            for (unsigned i = 0; i < 4; ++i) { threads[i] = CreateThread(NULL, 0, worker, fixture.queue, 0, NULL); assert(threads[i]); }
            assert(WaitForMultipleObjects(4, threads, TRUE, 10000) == WAIT_OBJECT_0);
            for (unsigned i = 0; i < 4; ++i) assert(CloseHandle(threads[i]));
        } else for (unsigned i = 0; i < fixture.length; ++i) assert(xl6ContinuationRunOne(fixture.queue));
        assert(xl6ContinuationQueueIsIdle(fixture.queue) && xl6ContinuationQueueActivationCount(fixture.queue) == 0);
        assert(fixture.continuationHooks.ran == fixture.length && fixture.continuationHooks.released == fixture.length);
        assert(xl6ContinuationQueueDestroy(fixture.queue)); fixture.queue = NULL;
        assert(fixture.acquired == 1 && fixture.denied == fixture.length);
        if (fixture.keepChildren) {
            assert(!fixture.unmapped); graph_children(&fixture);
            for (unsigned i = 0; i < fixture.length * 2; ++i) {
                api->Release(fixture.children[i]); fixture.children[i] = NULL;
                if (i + 1 < fixture.length * 2) graph_children(&fixture);
            }
        }
        assert(fixture.pins == 0 && fixture.dropped == 1 && fixture.unmapped == 1 && !xrtGetError()); balanced(&before);
    }
}
static void empty(void* data) { (void)data; }
static void lease_failures(void)
{
    for (unsigned round = 0; round < 100; ++round) {
        Fixture fixture; XL6_ContinuationTicket ticket = {0}, denied = {0}; xmemdebugsnapshot before; xerror* prior;
        xrtMemDebugSnapshot(&before); prepare(&fixture);
        fixture.queue = xl6ContinuationQueueCreate(1); assert(fixture.queue);
        prior = round & 1 ? xrtErrorCreate(XERR_STATE, "caller", 1, "prior") : NULL;
        if (round & 1) assert(prior);
        xrtSetErrorTake(prior); assert(xrtMemDebugFailAfter(0));
        assert(xl6ContinuationReserveOwned(fixture.queue, &ticket, empty, empty, NULL,
            &fixture.owner.base) == XL6_CONTINUATION_RESOURCE_FAILURE);
        assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        assert(prior ? xrtGetError() == prior : xrtErrorKind(xrtGetError()) == XERR_MEMORY); xrtClearError();
        assert(!ticket.queue && !ticket.codeLease && fixture.acquired == 1 && fixture.dropped == 1 &&
            fixture.pins == 0 && xl6ContinuationQueueIsIdle(fixture.queue));
        assert(xl6ContinuationReserveOwned(fixture.queue, &ticket, empty, empty, NULL,
            &fixture.owner.base) == XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationReserveOwned(fixture.queue, &denied, empty, empty, NULL,
            &fixture.owner.base) == XL6_CONTINUATION_FULL && !denied.queue && !denied.codeLease);
        assert(xl6ContinuationAbort(&ticket) && !ticket.queue && !ticket.codeLease);
        assert(fixture.acquired == 3 && fixture.dropped == 3 && fixture.pins == 0);
        xl6ContinuationQueueClose(fixture.queue); assert(xl6ContinuationQueueDestroy(fixture.queue)); balanced(&before);
    }
}
int main(int argc, char** argv)
{
    assert(argc == 2 && xrtMemDebugEnable(true)); api = xl6ClosureRuntimeV1Get();
    lifetimes(argv[1]); lease_failures();
    puts("Continuation scopes: 100 real DLL lifetimes, 200 run/release callback pairs, 350 inherited closures, 100 allocation-free successor abort/recommit handoffs, 25 four-native-thread drains, 100 actual root-lease OOM refusals and 100 capacity rollbacks; graph, queue-credit, diagnostics and allocator balance");
    assert(xrtMemDebugReset()); return 0;
}
