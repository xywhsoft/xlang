#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_executor.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>

typedef struct Root Root;
typedef struct Frame { XL6_ContinuationTicket ticket; Root* root; unsigned generation; } Frame;
struct Root {
    XL6_ContinuationExecutor* executor;
    XL6_ContinuationQueue* queue;
    xpromise* promise;
    xfuture* future;
    xfuture* dependency;
    atomic_int phase, runs, releases, acquires, drops;
    atomic_int* gate;
    Frame* pending;
    unsigned mode;
};
static int payload;
static void await_count(atomic_int* value, int count)
{
    xdeadline deadline = xrtDeadlineAfter(5000000);
    while (atomic_load_explicit(value, memory_order_acquire) != count) {
        assert(!xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
}
static void blocking_work(Root* root)
{
    /* No BlockingEnter/Leave annotation. These represent an ordinary foreign
     * call, a mandatory frame destructor, or host module retirement. */
    atomic_store_explicit(&root->phase, 1, memory_order_release);
    if (root->dependency) assert(xrtFutureWaitFor(root->dependency, 5000000) == XWAIT_OK);
    assert(xrtPromiseResolve(root->promise, &payload));
}
static int pin(void* data)
{ Root* root = data; atomic_fetch_add(&root->acquires, 1); return 1; }
static int reject_pin(void* data) { (void)data; return 0; }
static void drop(void* data)
{
    Root* root = data;
    if (root->mode == 2) blocking_work(root);
    if (root->mode == 4) {
        /* A final host hook is still executing even though its frame is gone. */
        assert(xl6ContinuationQueueActivationCount(root->queue) == 1);
        assert(!xl6ContinuationQueueIsIdle(root->queue));
        assert(!xl6ContinuationQueueDestroy(root->queue));
    }
    atomic_fetch_add_explicit(&root->drops, 1, memory_order_release);
}
static void publish(Frame* frame)
{
    xl6ContinuationReady(&frame->ticket);
    xl6ContinuationTransportReleased(&frame->ticket);
    xl6ContinuationCommit(&frame->ticket);
}
static void run(void* data);
static void release(void* data)
{
    Frame* frame = data; Root* root = frame->root;
    if (root->mode == 1) blocking_work(root);
    xrtFree(frame);
    atomic_fetch_add_explicit(&root->releases, 1, memory_order_release);
}
static void run(void* data)
{
    Frame* frame = data; Root* root = frame->root;
    atomic_fetch_add(&root->runs, 1);
    if (root->mode == 0 || (root->mode == 3 && frame->generation == 0)) blocking_work(root);
    if (root->mode == 3 && frame->generation == 0) {
        Frame* next = xrtCalloc(1, sizeof(*next)); assert(next);
        next->root = root; next->generation = 1;
        assert(xl6ContinuationReserveSuccessor(&frame->ticket, &next->ticket, run, release, next) == XL6_CONTINUATION_ADMITTED);
        publish(next);
    }
    if (root->mode == 5) {
        assert(xrtMemDebugFailAfter(0));
        assert(xl6ContinuationBlockingEnter(root->executor) == XL6_CONTINUATION_BLOCKING_ADMITTED);
        assert(xl6ContinuationBlockingEnter(root->executor) == XL6_CONTINUATION_BLOCKING_ADMITTED);
        xl6ContinuationBlockingLeave(root->executor); xl6ContinuationBlockingLeave(root->executor);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    }
}
static Frame* reserve(Root* root)
{
    Frame* frame = xrtCalloc(1, sizeof(*frame));
    XL6_CodeOwner owner = { sizeof(owner), root, pin, drop };
    assert(frame); frame->root = root;
    assert(xl6ContinuationExecutorReserveOwned(root->executor, &frame->ticket, run, release, frame, &owner) == XL6_CONTINUATION_ADMITTED);
    root->queue = frame->ticket.queue;
    return frame;
}
static int32_t concurrent_reserve(void* data)
{
    Root* root = data;
    await_count(root->gate, 1); root->pending = reserve(root);
    return 0;
}
static void finish(XL6_ContinuationExecutor* executor)
{
    xdeadline deadline = xrtDeadlineAfter(5000000);
    xl6ContinuationExecutorClose(executor);
    while (!xl6ContinuationExecutorDestroy(executor)) {
        assert(!xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
}
static void cohort(unsigned mode)
{
    Root roots[4] = {0}; unsigned i;
    XL6_ContinuationExecutor* executor = xl6ContinuationExecutorCreateGuaranteed(4, 1); assert(executor);
    for (i = 0; i < 4; ++i) {
        roots[i].executor = executor; roots[i].mode = mode;
        roots[i].promise = xrtPromiseCreate(&roots[i].future, NULL); assert(roots[i].promise);
    }
    for (i = 0; i < 3; ++i) roots[i].dependency = roots[i+1].future;
    /* Admit the next activation only AFTER the prior worker is blocked. All
     * workers can be inside ordinary calls/destructors/drop hooks at once. */
    for (i = 0; i < 4; ++i) {
        Frame* frame = reserve(&roots[i]); publish(frame);
        await_count(&roots[i].phase, 1);
    }
    finish(executor);
    for (i = 0; i < 4; ++i) {
        assert(atomic_load(&roots[i].runs) == (mode == 3 ? 2 : 1));
        assert(atomic_load(&roots[i].releases) == (mode == 3 ? 2 : 1));
        assert(atomic_load(&roots[i].acquires) == 1 && atomic_load(&roots[i].drops) == 1);
        xrtFutureDestroy(roots[i].future); xrtPromiseDestroy(roots[i].promise);
    }
}
int main(void)
{
    unsigned round, mode;
    assert(xrtMemDebugEnable(true));
    assert(!xl6ContinuationExecutorCreateGuaranteed(0, 1));
    assert(!xl6ContinuationExecutorCreateGuaranteed(1, 2));
    for (round = 0; round < 25; ++round) for (mode = 0; mode < 4; ++mode) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        cohort(mode); balanced(&before);
    }
    for (round = 0; round < 50; ++round) for (mode = 0; mode < 2; ++mode) {
        xmemdebugsnapshot before; Root a = {0}, b = {0}; Frame pending = {0}; XL6_CodeOwner owner;
        xerror* sentinel;
        xrtMemDebugSnapshot(&before);
        a.executor = b.executor = xl6ContinuationExecutorCreateGuaranteed(2, 1); assert(a.executor);
        a.promise = xrtPromiseCreate(&a.future, NULL); b.promise = xrtPromiseCreate(&b.future, NULL);
        assert(a.promise && b.promise); a.dependency = b.future; b.mode = 6;
        publish(reserve(&a)); await_count(&a.phase, 1);
        owner = (XL6_CodeOwner){ sizeof(owner), &b, pin, drop }; pending.root = &b;
        sentinel = round & 1 ? xrtErrorCreate(XERR_STATE, "caller", 61, "earlier diagnostic") : NULL;
        if (round & 1) assert(sentinel);
        xrtSetErrorTake(sentinel);
        /* Skip the new root's shared code-lease allocation. Preserve the
         * original Worker record, then XRT thread record: both allocations fail
         * before OS-thread launch, without racing another thread's allocator. */
        assert(xrtMemDebugFailAfter(mode + 1));
        assert(xl6ContinuationExecutorReserveOwned(b.executor, &pending.ticket, run, release, &pending, &owner) == XL6_CONTINUATION_RESOURCE_FAILURE);
        assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        assert(sentinel ? xrtGetError() == sentinel : xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtClearError();
        assert(pending.ticket.queue == NULL && atomic_load(&b.acquires) == 1 && atomic_load(&b.drops) == 1);
        assert(!atomic_load(&b.runs) && !atomic_load(&b.releases));
        assert(xl6ContinuationQueueActivationCount(a.queue) == 1);
        /* Previously accepted work still runs after another admission fails. */
        xl6ContinuationExecutorClose(a.executor);
        assert(xrtPromiseResolve(b.promise, &payload)); finish(a.executor);
        assert(atomic_load(&a.runs) == 1 && atomic_load(&a.releases) == 1 && atomic_load(&a.drops) == 1);
        xrtFutureDestroy(a.future); xrtFutureDestroy(b.future);
        xrtPromiseDestroy(a.promise); xrtPromiseDestroy(b.promise); balanced(&before);
    }
    for (round = 0; round < 50; ++round) {
        xmemdebugsnapshot before; Root roots[4] = {0}; xthread* threads[4]; unsigned i;
        atomic_int gate = 0; xfuture* future; xpromise* promise;
        XL6_ContinuationExecutor* executor;
        xrtMemDebugSnapshot(&before);
        executor = xl6ContinuationExecutorCreateGuaranteed(4, 1); assert(executor);
        promise = xrtPromiseCreate(&future, NULL); assert(promise);
        for (i = 0; i < 4; ++i) {
            roots[i].executor = executor; roots[i].gate = &gate; roots[i].dependency = future;
            roots[i].promise = xrtPromiseCreate(&roots[i].future, NULL); assert(roots[i].promise);
            threads[i] = xrtThreadCreate(concurrent_reserve, &roots[i], 0); assert(threads[i]);
        }
        atomic_store_explicit(&gate, 1, memory_order_release);
        for (i = 0; i < 4; ++i) {
            assert(xrtThreadWaitFor(threads[i], 5000000) == XWAIT_OK); xrtThreadDestroy(threads[i]);
        }
        xl6ContinuationExecutorClose(executor);
        for (i = 0; i < 4; ++i) publish(roots[i].pending);
        for (i = 0; i < 4; ++i) await_count(&roots[i].phase, 1);
        assert(xrtPromiseResolve(promise, &payload)); finish(executor);
        for (i = 0; i < 4; ++i) {
            assert(atomic_load(&roots[i].runs) == 1 && atomic_load(&roots[i].releases) == 1);
            assert(atomic_load(&roots[i].acquires) == 1 && atomic_load(&roots[i].drops) == 1);
            xrtFutureDestroy(roots[i].future); xrtPromiseDestroy(roots[i].promise);
        }
        xrtFutureDestroy(future); xrtPromiseDestroy(promise); balanced(&before);
    }
    for (round = 0; round < 25; ++round) for (mode = 0; mode < 3; ++mode) {
        xmemdebugsnapshot before; Root root = {0}; Frame* frame; Frame extra = {0};
        xrtMemDebugSnapshot(&before); root.mode = mode == 2 ? 5 : 4;
        root.executor = xl6ContinuationExecutorCreateGuaranteed(1, 1); assert(root.executor);
        frame = reserve(&root);
        {
            XL6_CodeOwner denied = { sizeof(denied), NULL, reject_pin, drop };
            assert(xrtGetError() == NULL);
            assert(xl6ContinuationExecutorReserveOwned(root.executor, &extra.ticket, run, release,
                &extra, &denied) == XL6_CONTINUATION_CLOSED);
            assert(xrtGetError() == NULL && extra.ticket.queue == NULL);
        }
        assert(xl6ContinuationExecutorReserve(root.executor, &extra.ticket, run, release, &extra) == XL6_CONTINUATION_FULL);
        xl6ContinuationExecutorClose(root.executor);
        assert(xl6ContinuationExecutorReserve(root.executor, &extra.ticket, run, release, &extra) == XL6_CONTINUATION_CLOSED);
        if (mode == 0) { assert(xl6ContinuationAbort(&frame->ticket)); xrtFree(frame); }
        else publish(frame);
        finish(root.executor); assert(atomic_load(&root.drops) == 1); balanced(&before);
    }
    puts("guaranteed progress: 100 four-root blocking cohorts (run/destructor/host-drop/successor), 50 concurrent four-root admissions, 100 real admission allocation failures, 75 capacity/close/abort/nested/diagnostic-free refusal cases; balanced memory");
    return 0;
}
