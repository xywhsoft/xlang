#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_SEM
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void cell_thread_prepare(void), cell_thread_finish(void);
extern void* cell_thread_raw_new(void);
extern void cell_thread_raw_retain(void*), cell_thread_raw_release(void*);
extern int32_t cell_thread_raw_count(void*);
extern void* cell_thread_factory(void);
extern void* cell_thread_reader(void*);
extern void cell_thread_read(void*), cell_thread_release(void*);
enum { WORKERS = 8 };
static xatomic32 drops;
static xsem* start;
typedef struct Worker { void* owner; unsigned count; int mode; } Worker;
void cell_thread_drop(void) { xrtAtomic32FetchAdd(&drops, 1, XMEMORY_RELAXED); }
static int32 run_worker(void* data)
{
    Worker* worker = (Worker*)data; unsigned i;
    assert(xrtSemWaitFor(start, 15000000) == XWAIT_OK);
    for (i = 0; i < worker->count; ++i) {
        if (worker->mode == 0) cell_thread_raw_retain(worker->owner);
        else if (worker->mode == 1) cell_thread_raw_release(worker->owner);
        else if (worker->mode == 2) { cell_thread_raw_retain(worker->owner); cell_thread_raw_release(worker->owner); }
        else if (worker->mode == 3) {
            void* reader = cell_thread_reader(worker->owner); cell_thread_read(reader); cell_thread_release(reader);
        } else { assert(worker->mode == 4); cell_thread_read(worker->owner); cell_thread_release(worker->owner); }
    }
    assert(!xrtGetError()); return 0;
}
static void run_group(Worker* work)
{
    xthread* threads[WORKERS]; unsigned i;
    for (i = 0; i < WORKERS; ++i) { threads[i] = xrtThreadCreate(run_worker, &work[i], 0); assert(threads[i]); }
    for (i = 0; i < WORKERS; ++i) assert(xrtSemPost(start));
    for (i = 0; i < WORKERS; ++i) {
        assert(xrtThreadWaitFor(threads[i], 15000000) == XWAIT_OK); xrtThreadDestroy(threads[i]);
    }
}
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    xmemdebugsnapshot initial, before, after; Worker work[WORKERS]; unsigned i, round;
    unsigned repetitions = smoke ? 1000u : 25000u, cycles = smoke ? 2u : 30u;
    void *cell, *factory; uint32 beforeDrops;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    xrtAtomic32Init(&drops, 0); cell_thread_prepare();
    start = xrtSemCreate(0, WORKERS); assert(start);
    cell = cell_thread_raw_new(); assert(cell);
    for (i = 0; i < WORKERS; ++i) work[i] = (Worker){cell, repetitions, 0};
    run_group(work);
    // Inspect only after every worker has joined. Never read a live refcount.
    fprintf(stderr, "cell retain fanout: expected=%u actual=%d\n", 1u + WORKERS * repetitions, (int)cell_thread_raw_count(cell));
    if (cell_thread_raw_count(cell) != (int32_t)(1u + WORKERS * repetitions)) return 2;
    for (i = 0; i < WORKERS; ++i) work[i].mode = 1;
    run_group(work); assert(cell_thread_raw_count(cell) == 1);
    for (i = 0; i < WORKERS; ++i) work[i].mode = 2;
    run_group(work); assert(cell_thread_raw_count(cell) == 1); cell_thread_raw_release(cell);
    for (round = 0; round < cycles; ++round) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        beforeDrops = xrtAtomic32Load(&drops, XMEMORY_RELAXED); factory = cell_thread_factory();
        for (i = 0; i < WORKERS; ++i) work[i] = (Worker){factory, smoke ? 10u : 200u, 3};
        run_group(work); assert(xrtAtomic32Load(&drops, XMEMORY_RELAXED) == beforeDrops);
        for (i = 0; i < WORKERS; ++i) work[i] = (Worker){cell_thread_reader(factory), 1, 4};
        cell_thread_release(factory); assert(xrtAtomic32Load(&drops, XMEMORY_RELAXED) == beforeDrops);
        run_group(work); assert(xrtAtomic32Load(&drops, XMEMORY_RELAXED) == beforeDrops + 1);
        if (!smoke) continuation_owner_fault_balanced(&before);
    }
    xrtSemDestroy(start); cell_thread_finish();
    if (!smoke) {
        continuation_owner_fault_balanced(&initial); xrtMemDebugSnapshot(&after);
        printf("cell thread lifetime: %u workers, %u raw retain/release operations, %u source readers, %u final-owner races, %llu balanced allocations\n",
            WORKERS, 4u * WORKERS * repetitions, WORKERS * (200u + 1u) * cycles, cycles,
            (unsigned long long)(after.AllocCount - initial.AllocCount));
    } else printf("cell thread C-host smoke: %u workers, sibling/nested binding identity and %u final-owner races\n", WORKERS, cycles);
    return 0;
}
