#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#define XRT_MODULE_SEM
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern void pool_lifecycle_cycle(int);
extern void pool_run_unload(void);
extern unsigned pool_allocation_faults(void);
extern unsigned pool_global_construction_fault_probe(void);
extern unsigned pool_local_construction_fault_probe(int);
static unsigned constructionChildren, constructionDrops, constructionHandles;
void construction_child_created(void) { ++constructionChildren; }
void construction_child_dropped(void) { ++constructionDrops; }
void* construction_native_open(int64_t number)
{
    int64_t* handle = xrtCalloc(1, sizeof(*handle));
    if (handle != NULL) { *handle = number; ++constructionHandles; }
    return handle;
}
void construction_native_close(void* handle)
{
    assert(handle != NULL && constructionHandles != 0);
    --constructionHandles; xrtFree(handle);
}
extern void pool_initializer_rollback(void);
static xsem *entered, *proceed;
static xthread* unloader;
static xatomic32 owners = {0}, jobs = {0}, self = {0}, rejected = {0};
void pool_count_owner(int delta) { xrtAtomic32FetchAdd(&owners, delta, XMEMORY_SEQ_CST); }
int pool_owner_count(void) { return (int)xrtAtomic32Load(&owners, XMEMORY_SEQ_CST); }
void pool_count_job(void) { xrtAtomic32FetchAdd(&jobs, 1, XMEMORY_SEQ_CST); }
void pool_count_self(void) { xrtAtomic32FetchAdd(&self, 1, XMEMORY_SEQ_CST); }
void pool_count_rejected(void) { xrtAtomic32FetchAdd(&rejected, 1, XMEMORY_SEQ_CST); }
void pool_pause(void) { xrtSleep(1); }
void pool_sync_block(void) { assert(xrtSemPost(entered) && xrtSemWaitFor(proceed, 15000000) == XWAIT_OK); }
void pool_sync_entered(void) { assert(xrtSemWaitFor(entered, 15000000) == XWAIT_OK); }
void pool_sync_release(void) { assert(xrtSemPost(proceed)); }
static int32 unload_entry(void* data) { (void)data; pool_run_unload(); return 0; }
void pool_sync_start_unload(void) { assert(!unloader); unloader = xrtThreadCreate(unload_entry, NULL, 0); assert(unloader); }
void pool_sync_join_unload(void) { assert(xrtThreadWaitFor(unloader, 15000000) == XWAIT_OK); xrtThreadDestroy(unloader); unloader = NULL; }
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after; int i;
    unsigned jobStart, selfStart, rejectStart;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    int selectedConstruction = argc == 3 && strcmp(argv[1], "--construction-variant") == 0
        ? atoi(argv[2]) : -1;
    int constructionProbe = argc == 2 && strcmp(argv[1], "--global-construction-fault") == 0;
    assert(smoke || ((argc == 1 || constructionProbe || (selectedConstruction >= 0 && selectedConstruction < 5)) && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    entered = xrtSemCreate(0, 1); proceed = xrtSemCreate(0, 1); assert(entered && proceed);
    pool_initializer_rollback();
    pool_lifecycle_cycle(0);
    if (!smoke) {
        unsigned constructionFaults;
        xrtMemDebugSnapshot(&before);
        constructionFaults = pool_global_construction_fault_probe();
        xrtMemDebugSnapshot(&after);
        assert(constructionFaults > 1);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        printf("class construction: %u real allocation failures, %llu balanced allocations, clean retry after every rollback\n",
            constructionFaults, (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    if (!smoke) {
        int kind;
        for (kind = 0; kind < 5; ++kind) {
            unsigned constructionFaults;
            if (selectedConstruction >= 0 && selectedConstruction != kind) continue;
            xrtMemDebugSnapshot(&before);
            constructionFaults = pool_local_construction_fault_probe(kind);
            xrtMemDebugSnapshot(&after);
            assert(constructionFaults != 0 && constructionChildren == constructionDrops && !constructionHandles);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
            printf("class variant %d: %u allocation faults, %llu balanced allocations\n", kind,
                constructionFaults, (unsigned long long)(after.AllocCount - before.AllocCount));
        }
    }
    if (!smoke) xrtMemDebugSnapshot(&before);
    jobStart = xrtAtomic32Load(&jobs, XMEMORY_SEQ_CST);
    selfStart = xrtAtomic32Load(&self, XMEMORY_SEQ_CST);
    rejectStart = xrtAtomic32Load(&rejected, XMEMORY_SEQ_CST);
    for (i = 0; i < 30; ++i) pool_lifecycle_cycle(i);
    assert(xrtAtomic32Load(&jobs, XMEMORY_SEQ_CST) - jobStart == 210);
    assert(xrtAtomic32Load(&self, XMEMORY_SEQ_CST) - selfStart == 60);
    assert(xrtAtomic32Load(&rejected, XMEMORY_SEQ_CST) - rejectStart == 300);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        printf("module pool: %llu balanced allocations, 210 worker/callback unloads, 60 self-unload rejections, 300 rejected nested/destructor submissions\n",
            (unsigned long long)(after.AllocCount - before.AllocCount));
        printf("module pool: %u real initialization/submission allocation faults\n", pool_allocation_faults());
    }
    assert(xrtSemDestroy(entered) && xrtSemDestroy(proceed)); entered = proceed = NULL;
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.InvalidFreeCount == after.InvalidFreeCount && initial.DoubleFreeCount == after.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("module pool: generated quiescence precedes global cleanup; raising cleanup, retry, reactivation and callback join passed");
    return 0;
}
