#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_value_iterator.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void iteration_projection_prepare(void), iteration_projection_finish(void);
extern void iteration_projection_wait_idle(void);
extern bool iteration_projection_idle(void);
extern xvalue* iteration_projection_new(int64_t);
extern int64_t iteration_projection_pair(int64_t, int64_t), iteration_projection_single(int64_t), iteration_projection_cells(int64_t);
extern int64_t iteration_projection_consume(int64_t, bool);
extern xfuture* iteration_projection_launch(int, xfuture*);
extern void iteration_projection_produce(xpromise*, int, int64_t*);
extern void* iteration_projection_generator(int);
extern int iteration_projection_next(void*, int64_t*);
extern void iteration_projection_close(void*);
static int smoke, kind, shape, selectedIteration, selectedFault, raisingId, raisingMode;
static unsigned advances, visits, caught, finalized, created, destroyed, active, drops[3];
static unsigned armed, everArmed, windowAllocations, executions, faults, resumedFaults, closeCases;
static xvaluekey currentKey;
static xvalue* currentItem; // Borrowed until the next real advance; never a test-owned root.
static xfuture* activeOutput;
static void end_window(void)
{
    if (!armed) return;
    windowAllocations = continuation_owner_fault_end(selectedFault); armed = 0;
}
static XL6_ValueIterator* iteration_projection_cursor_create(const xvalue* source)
{
    XL6_ValueIterator* cursor = xl6ValueIteratorRuntimeV2Get()->Create(source);
    assert(cursor && !created && !active); ++created; ++active; return cursor;
}
static bool iteration_projection_cursor_advance(XL6_ValueIterator* cursor, bool* found)
{
    bool ok = xl6ValueIteratorRuntimeV2Get()->Advance(cursor,found);
    assert(active == 1 && !armed && ok);
    if (*found) {
        const XL6_ValueIteratorView* view = xl6ValueIteratorRuntimeV2Get()->View(cursor);
        ++advances; currentKey.Type = view->key.Type; currentItem = view->item;
        if (currentKey.Type == XVALUE_KEY_STRING) currentKey.String = (xstrview){view->key.String.Data,view->key.String.Size};
        else if (currentKey.Type == XVALUE_KEY_INT) currentKey.Integer = view->key.Integer;
        else currentKey.Index = view->key.Index;
        if (!smoke && (int)advances == selectedIteration) {
            assert(!everArmed); armed = everArmed = 1; continuation_owner_fault_arm(selectedFault);
        }
    }
    return ok;
}
static void iteration_projection_cursor_destroy(XL6_ValueIterator* cursor)
{
    assert(cursor && active == 1 && !destroyed);
    xl6ValueIteratorRuntimeV2Get()->Destroy(cursor); --active; ++destroyed;
}
const XL6_ValueIteratorRuntimeV2* iteration_projection_iterator_runtime(void)
{
    static XL6_ValueIteratorRuntimeV2 api;
    if (!api.Create) { api = *xl6ValueIteratorRuntimeV2Get(); api.Create = iteration_projection_cursor_create;
        api.Advance = iteration_projection_cursor_advance; api.Destroy = iteration_projection_cursor_destroy; }
    return &api;
}
xvalue* iteration_projection_input(int64_t requested)
{
    xvalue* source; int i;
    assert(requested == kind && !created);
    source = kind == 0 ? xrtValueArray() : kind == 1 ? xrtValueIntMap() : kind == 2 ? xrtValueObject() : xrtValueSet();
    assert(source);
    for (i = 1; i <= 2; ++i) {
        xvalue* item = kind == 3 ? xrtValueInt(i + 10) : iteration_projection_new(i);
        assert(item);
        if (kind == 0) assert(xrtValueArrayAppendNew(source, item));
        else if (kind == 1) assert(xrtValueIntMapSetNew(source, i * 10000000001LL, item));
        else if (kind == 2) assert(xrtValueObjectSetNew(source, (xstrview){i == 1 ? "alpha" : "beta", i == 1 ? 5u : 4u}, item));
        else assert(xrtValueSetAddNew(source, item));
    }
    // Export returns this sole owner directly into the source loop. Its
    // expression owner is released after cursor creation, before any advance.
    return source;
}
void iteration_projection_visit(xvalue* key, xvalue* item)
{
    end_window(); assert(active == 1 && visits + 1 == advances && key && item && !drops[1] && !drops[2]);
    assert(!everArmed || selectedFault < 0); // A failed acquisition never reaches the body.
    if (shape == 1 && kind != 2) assert(key == currentItem && item == currentItem);
    else {
        if (currentKey.Type == XVALUE_KEY_STRING) {
            xstrview actual = {0}; assert(xrtValueGetString(key, &actual));
            assert(actual.Size == currentKey.String.Size && !memcmp(actual.Data, currentKey.String.Data, actual.Size));
        } else {
            int64 actual = -1, expected = currentKey.Type == XVALUE_KEY_INT ? currentKey.Integer :
                currentKey.Type == XVALUE_KEY_INDEX ? (int64)currentKey.Index : (int64)advances - 1;
            assert(xrtValueGetInt(key, &actual) && actual == expected);
        }
        assert(shape == 1 ? item == key : item == currentItem);
    }
    ++visits;
}
int iteration_projection_drop_mode(int id)
{
    // On frame destruction the current binding may outlive the cursor's
    // backing reference. Both owners must be gone before close completes.
    assert(id >= 1 && id <= 2 && !drops[id] && created == 1 && destroyed <= 1);
    if (activeOutput) assert(!xrtFutureDone(activeOutput));
    ++drops[id]; return id == raisingId ? raisingMode : 0;
}
void iteration_projection_caught(void)
{
    end_window(); assert(!active && destroyed == 1 && !caught && selectedFault >= 0);
    assert(kind == 3 || (drops[1] == 1 && drops[2] == 1)); ++caught;
}
void iteration_projection_finally(void)
{
    assert(!active && !finalized && destroyed == 1);
    assert(kind == 3 || (drops[1] == 1 && drops[2] == 1)); ++finalized;
}
xfuture* iteration_projection_future(void) { assert(0 && "reachability helper is not executed"); return NULL; }
static void begin_case(int collection, int form, int iteration, int fault, int dropId, int dropMode)
{
    kind = collection; shape = form; selectedIteration = iteration; selectedFault = fault;
    raisingId = dropId; raisingMode = dropMode;
    advances = visits = caught = finalized = created = destroyed = active = 0;
    armed = everArmed = windowAllocations = 0; memset(drops, 0, sizeof(drops));
    activeOutput = NULL;
}
static unsigned finish_case(const xmemdebugsnapshot* before, unsigned expectedVisits, int entered)
{
    if (finalized != (unsigned)entered) fprintf(stderr,
        "incomplete close: kind=%d shape=%d iteration=%d fault=%d visits=%u caught=%u finally=%u cursor=%u/%u drops=%u/%u\n",
        kind, shape, selectedIteration, selectedFault, visits, caught, finalized, created, destroyed, drops[1], drops[2]);
    assert(!armed && !active && created == (unsigned)entered && destroyed == (unsigned)entered);
    assert(finalized == (unsigned)entered && caught == (unsigned)(selectedFault >= 0));
    assert(visits == expectedVisits && !xrtGetError());
    assert(drops[1] == (unsigned)(entered && kind != 3) && drops[2] == drops[1]);
    if (!smoke) {
        if (selectedIteration > 0) assert(everArmed);
        continuation_owner_fault_balanced(before);
    }
    if (selectedFault >= 0) ++faults;
    ++executions; return windowAllocations;
}
static unsigned sync_case(int collection, int form, int flow, int iteration, int fault, int dropId, int dropMode)
{
    xmemdebugsnapshot before; int64_t value;
    if (!smoke) xrtMemDebugSnapshot(&before);
    begin_case(collection, form, iteration, fault, dropId, dropMode);
    value = form == 0 ? iteration_projection_pair(kind, flow) : form == 1 ? iteration_projection_single(kind) : iteration_projection_cells(kind);
    assert(value == (fault >= 0 ? -100 : flow == 1 || flow == 3 ? 1 : 2));
    return finish_case(&before, fault >= 0 ? (unsigned)iteration - 1u : flow == 1 || flow == 3 ? 1u : 2u, 1);
}
static unsigned async_case(int collection, int ready, int iteration, int fault, int dropId, int dropMode, int producerMode)
{
    xmemdebugsnapshot before; xfuture *input = NULL, *output; xpromise* promise;
    xfutureresult result; int64_t payload = 7;
    if (!smoke) xrtMemDebugSnapshot(&before);
    begin_case(collection, 0, iteration, fault, dropId, dropMode);
    promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
    if (ready) iteration_projection_produce(promise, producerMode, &payload);
    output = iteration_projection_launch(kind, input); activeOutput = output;
    if(!ready && fault<0)assert(!iteration_projection_idle());
    if (!ready) iteration_projection_produce(promise, producerMode, &payload);
    /* Promise completion queues the continuation; it does not execute it on
     * this producer stack. Observe the actual terminal Future first. */
    assert(xrtFutureWait(output) == 0);
    assert(xrtFutureResult(output, &result) && result.State == XFUTURE_RESOLVED && result.Value && !result.Error);
    assert(*(int64_t*)result.Value == (fault >= 0 ? -100 : 2));
    if (fault >= 0 && !ready && iteration == 2) ++resumedFaults;
    activeOutput = NULL; xrtFutureDestroy(output); xrtPromiseDestroy(promise); xrtFutureDestroy(input);
    iteration_projection_wait_idle();
    return finish_case(&before, fault >= 0 ? (unsigned)iteration - 1u : 2u, 1);
}
static unsigned generator_case(int collection, int iteration, int fault, int dropId, int dropMode, int stop)
{
    xmemdebugsnapshot before; void* generator; int count = 0, next; int64_t value = 0;
    if (!smoke) xrtMemDebugSnapshot(&before);
    begin_case(collection, 0, iteration, fault, dropId, dropMode);
    generator = iteration_projection_generator(kind); assert(generator);
    while (stop < 0 || count < stop) {
        next = iteration_projection_next(generator, &value);
        if (!next) break;
        ++count; assert(value == (fault >= 0 && count == iteration ? -100 : count));
    }
    if (stop >= 0) ++closeCases;
    iteration_projection_close(generator);
    return finish_case(&before, stop == 0 ? 0u : fault >= 0 ? (unsigned)iteration - 1u : stop == 1 ? 1u : 2u, stop != 0);
}
static void source_close_case(int collection, int stop)
{
    xmemdebugsnapshot before; int64_t result;
    if (!smoke) xrtMemDebugSnapshot(&before);
    begin_case(collection, 0, -1, -1, 0, 0);
    result = iteration_projection_consume(collection, stop != 0);
    assert(result == (stop ? 1 : 3));
    finish_case(&before, stop ? 1u : 2u, 1);
}
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, after; unsigned round, count, fault;
    int collection, form, flow, iteration, ready, producer, dropId, dropMode, stop;
    smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    iteration_projection_prepare();
    for (round = 0; round < (smoke ? 1u : 10u); ++round) for (collection = 0; collection < 4; ++collection) {
        for (flow = 0; flow < 4; ++flow) sync_case(collection, 0, flow, -1, -1, 0, 0);
        sync_case(collection, 1, 0, -1, -1, 0, 0); sync_case(collection, 2, 0, -1, -1, 0, 0);
        for (ready = 0; ready < 2; ++ready) for (producer = 0; producer < 3; ++producer)
            async_case(collection, ready, -1, -1, 0, 0, producer);
        generator_case(collection, -1, -1, 0, 0, -1);
        if (smoke) continue;
        for (iteration = 1; iteration <= 2; ++iteration) {
            for (form = 0; form < 3; ++form) {
                count = sync_case(collection, form, 0, iteration, -1, 0, 0);
                assert((form == 1 && collection != 2) ? count == 0 : count > 0);
                for (fault = 0; fault < count; ++fault) for (dropId = 0; dropId <= (collection == 3 ? 0 : 2); ++dropId)
                    for (dropMode = 1; dropMode <= (dropId ? 2 : 1); ++dropMode)
                        sync_case(collection, form, 0, iteration, (int)fault, dropId, dropMode);
            }
            for (ready = 0; ready < 2; ++ready) for (producer = 0; producer < 3; ++producer) {
                count = async_case(collection, ready, iteration, -1, 0, 0, producer); assert(count);
                for (fault = 0; fault < count; ++fault) for (dropId = 0; dropId <= (collection == 3 ? 0 : 2); ++dropId)
                    for (dropMode = 1; dropMode <= (dropId ? 2 : 1); ++dropMode)
                        async_case(collection, ready, iteration, (int)fault, dropId, dropMode, producer);
            }
            count = generator_case(collection, iteration, -1, 0, 0, -1); assert(count);
            for (fault = 0; fault < count; ++fault) for (dropId = 0; dropId <= (collection == 3 ? 0 : 2); ++dropId)
                for (dropMode = 1; dropMode <= (dropId ? 2 : 1); ++dropMode)
                    generator_case(collection, iteration, (int)fault, dropId, dropMode, -1);
        }
    }
    // Keep the close assertion mandatory, but run the independent acquisition
    // audit first so a known close-CFG defect cannot mask allocation failures.
    printf("dynamic iteration acquisition phase: %u executions, %u real faults, %u after pending resume; memory checks: %s\n",
        executions, faults, resumedFaults, smoke ? "disabled for C-host smoke" : "every completed case balanced"); fflush(stdout);
    for (round = 0; round < (smoke ? 1u : 10u); ++round)
        for (collection = 0; collection < 4; ++collection)
            for (stop = 0; stop <= 1; ++stop) source_close_case(collection, stop);
    for (round = 0; round < (smoke ? 1u : 10u); ++round)
        for (collection = 0; collection < 4; ++collection)
            for (stop = 0; stop <= 3; ++stop) generator_case(collection, -1, -1, 0, 0, stop);
    iteration_projection_finish();
    if (!smoke) {
        continuation_owner_fault_balanced(&initial); xrtMemDebugSnapshot(&after);
        assert(faults && resumedFaults);
        printf("dynamic iteration: %u executions, %u real projection/cell allocation faults (%u after pending resume), %u generator early/exhausted closes, %llu balanced allocations\n",
            executions, faults, resumedFaults, closeCases, (unsigned long long)(after.AllocCount - initial.AllocCount));
    } else printf("dynamic iteration C-host smoke: %u executions, %u generator closes\n", executions, closeCases);
    return 0;
}
