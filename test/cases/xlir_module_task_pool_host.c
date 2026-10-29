#include "release/include/xl_clock.h"
#include <assert.h>
#include <stdio.h>
#include "xlir_module_task_pool.c"
extern void pool_sync_block(void);
extern void pool_sync_entered(void);
extern void pool_sync_release(void);
extern void pool_sync_start_unload(void);
extern void pool_sync_join_unload(void);
extern void pool_pause(void);
extern void pool_count_owner(int);
extern int pool_owner_count(void);
extern void pool_count_job(void);
extern void pool_count_self(void);
extern void pool_count_rejected(void);
extern bool xrtMemDebugFailAfter(uint64_t);
extern bool xrtMemDebugFailTriggered(void);
extern void xrtMemDebugFailClear(void);
static int raisingDrop;
static int failConstructorAfterSubmit;
static int unloadOk, unloadCode;
static unsigned faults;
static int pool_retire_until_settled(void)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        XL6_ModuleRetireResult result=xl_script_module_retire_step();
        if(result==XL6_MODULE_RETIRE_READY)return 1;
        if(result==XL6_MODULE_RETIRE_FAILED)return 0;
        if(result!=XL6_MODULE_RETIRE_BUSY||xrtGetError()||xl6DeadlineExpiredV1(deadline))
            fprintf(stderr,"pool retire result=%d initialized=%d error=%d code=%d expired=%d\n",
                result,xlir_module_initialized,xrtGetError()?(int)xrtErrorKind(xrtGetError()):0,
                xrtGetError()?xrtErrorCode(xrtGetError()):0,(int)xl6DeadlineExpiredV1(deadline));
        assert(result==XL6_MODULE_RETIRE_BUSY&&!xrtGetError()&&!xl6DeadlineExpiredV1(deadline));
        xrtThreadYield();
    }
}
static XL6_TaskOutcome callback_task(xcancel*, void*, XL6_TaskResult*);
static void final_callback(void*,void*);
static void callback_drop(void* data,void* user) { (void)data; (void)user; }
static bool callback_trace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data; (void)visit; (void)context; return true; }
static xfuture* callback_submit(bool final)
{
    /* This native callback deliberately exercises the opaque legacy API. */
    void* frame=(xl6TaskRuntimeV1Get()->Allocate)(xlir_task_owner,1,callback_task,final?final_callback:callback_drop,callback_trace);
    xfuture* future;
    if(!frame)return NULL;
    future=xlir_async_pool_submit(frame,NULL,XL6_FUTURE_PAYLOAD_VALUE);
    if(!future)xlir_task_discard(frame);
    return future;
}
void pool_test_created(void)
{
    pool_count_owner(1);
    if (failConstructorAfterSubmit) {
        xfuture* future = callback_submit(false);
        xerror* error;
        assert(future && xlir_module_initialized == 1);
        xrtFutureDestroy(future);
        /* The constructor did not commit its external resource. */
        pool_count_owner(-1);
        error = xrtErrorCreate(XERR_STATE, "pool-test", 73, "constructor failed after task submission");
        assert(error); xl6ErrorScopePropagateFailure((xl6_error_result){ false, error });
    }
}
void pool_test_dropped(void)
{
    pool_count_owner(-1);
    /* The resource gate still exists while global cleanup executes, but no
     * submission is allowed to recreate a pool from a destructor. */
    assert(xlir_async_pool && xl6TaskRuntimeV1Get()->PoolIsQuiescent(xlir_async_pool));
    assert(callback_submit(false) == NULL);
    assert(xrtGetError()); xrtClearError(); pool_count_rejected();
    if (raisingDrop) {
        xerror* error = xrtErrorCreate(XERR_STATE, "pool-test", 91, "global destroy failed");
        assert(error);
        xl6ErrorScopePropagateFailure((xl6_error_result){ false, error });
    }
}
void pool_test_job(int64_t mode)
{
    assert(pool_owner_count() == 1 && pool_test_read() == 42);
    if (mode == 1) {
        assert(xl_script_module_retire_step() == XL6_MODULE_RETIRE_BUSY && !xrtGetError());
        assert(xlir_module_initialized == 2 && pool_owner_count() == 1);
        pool_count_self();
    }
    if (mode >= 0) pool_sync_block();
    if (mode == 2) {
        pool_test_start_spawn(-1);
        assert(xrtGetError()); xrtClearError(); pool_count_rejected();
    }
    assert(pool_owner_count() == 1 && pool_test_read() == 42);
    pool_count_job();
}
void pool_run_unload(void)
{
    unloadOk = pool_retire_until_settled();
    unloadCode = xrtGetError() ? xrtErrorCode(xrtGetError()) : 0;
    xrtClearError();
}
static XL6_TaskOutcome callback_task(xcancel* cancel, void* data, XL6_TaskResult* value)
{
    (void)cancel; (void)data;
    value->Value = NULL; value->Destroy = NULL; value->DestroyData = NULL;
    return XL6_TASK_SUCCESS;
}
static void final_callback(void* value, void* data)
{
    (void)value; (void)data;
    /* Runs in the task frame Destroy callback, after the job's execution. */
    pool_test_job(2);
}
static bool pool_is_closing(void)
{
    return xl6TaskRuntimeV1Get()->PoolIsClosing(xlir_async_pool);
}
void pool_initializer_rollback(void)
{
    failConstructorAfterSubmit = 1;
    assert(!xlir_module_init() && xrtGetError() && xrtErrorCode(xrtGetError()) == 73);
    xrtClearError(); failConstructorAfterSubmit = 0;
    assert(pool_retire_until_settled()&&!xrtGetError());
    assert(!xlir_module_initialized && !pool_owner_count() && !xlir_async_pool);
    assert(!xlir_async_pool);
}
void pool_lifecycle_cycle(int iteration)
{
    int family, mode, spin;
    for (family = 0; family < 2; ++family) for (mode = 0; mode < 3; ++mode) {
        assert(xlir_module_init() && pool_owner_count() == 1 && xl6TaskRuntimeV1Get()->PoolIsQuiescent(xlir_async_pool));
        raisingDrop = iteration & 1;
        if (family == 0) pool_test_start_async(mode);
        else pool_test_start_spawn(mode);
        assert(!xrtGetError()); pool_sync_entered();
        pool_sync_start_unload();
        for (spin = 0; spin < 10000 && !pool_is_closing(); ++spin) pool_pause();
        assert(spin < 10000 && pool_owner_count() == 1);
        pool_sync_release(); pool_sync_join_unload();
        assert(unloadOk == !raisingDrop && unloadCode == (raisingDrop ? 91 : 0));
        assert(pool_owner_count() == 0 && xlir_module_initialized == 0);
        assert(xlir_async_pool == NULL);
        assert(xl_script_module_unit_checked() && !xrtGetError());
    }
    {
        xfuture* future;
        assert(xlir_module_init() && pool_owner_count() == 1);
        raisingDrop = iteration & 1;
        future = callback_submit(true); assert(future);
        xrtFutureDestroy(future);
        pool_sync_entered(); pool_sync_start_unload();
        for (spin = 0; spin < 10000 && !pool_is_closing(); ++spin) pool_pause();
        assert(spin < 10000 && pool_owner_count() == 1);
        pool_sync_release(); pool_sync_join_unload();
        assert(unloadOk == !raisingDrop && unloadCode == (raisingDrop ? 91 : 0));
        assert(pool_owner_count() == 0 && !xlir_async_pool);
    }
    raisingDrop = 0;
}
unsigned pool_allocation_faults(void)
{
    unsigned index;
    /* This lane tests the module resource gate, not class construction. The
     * latter has a separate reproducer below until its IR transaction closes. */
    for (index = 0; index < 1; ++index) {
        int ok; bool triggered;
        assert(xrtMemDebugFailAfter(index));
        ok = xlir_module_init(); triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) ++faults;
        if (!ok) {
            assert(xrtGetError()); xrtClearError();
            assert(pool_retire_until_settled() && !xrtGetError());
        }
        if (ok) assert(xl_script_module_unit_checked());
        assert(!xrtGetError() && !xlir_module_initialized && !pool_owner_count());
        assert(!xlir_async_pool);
    }
    /* Frame/copy, lazy pool creation and submission allocation failures.
     * The nonblocking worker observes globals before every unload. */
    for (index = 0; index < 14; ++index) {
        bool triggered;
        assert(xlir_module_init() && xrtMemDebugFailAfter(index));
        pool_test_start_async(-1);
        triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (triggered) ++faults;
        xrtClearError();
        assert(pool_retire_until_settled() && !xrtGetError());
        assert(!xlir_module_initialized && !pool_owner_count() && !xlir_async_pool);
    }
    return faults;
}

/* Sweep the complete global construction, including pool gate, object backing,
 * tag boxing/insertion, field boxing/insertion and the explicit create body.
 * Every failed attempt must leave the module retryable; success ends the sweep. */
unsigned pool_global_construction_fault_probe(void)
{
    unsigned index;
    for (index = 0; index < 128; ++index) {
        int ok, triggered;
        assert(!xlir_module_initialized && !pool_owner_count() && !xlir_async_pool);
        assert(xrtMemDebugFailAfter(index));
        ok = xlir_module_init();
        triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (!triggered) {
            assert(ok && !xrtGetError());
            assert(xl_script_module_unit_checked());
            assert(!xlir_module_initialized && !pool_owner_count() && !xlir_async_pool);
            return index;
        }
        assert(!ok && xrtGetError()); xrtClearError();
        assert(pool_retire_until_settled() && !xrtGetError());
        assert(!xlir_module_initialized && !pool_owner_count() && !xlir_async_pool);
    }
    assert(!"construction fault sweep never reached a successful allocation budget");
    return 0;
}

/* Module initialization is outside these local-construction fault windows.
 * All five variants must converge to success, with no swallowed allocation
 * failure and with their already-constructed child/native owners rolled back. */
unsigned pool_local_construction_fault_probe(int kind)
{
    unsigned index;
    assert(xlir_module_init());
    for (index = 0; index < 256; ++index) {
        int64_t result;
        int triggered;
        assert(xrtMemDebugFailAfter(index));
        result = pool_test_construct_variant(kind);
        triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (!triggered) {
            assert(result == 1 && !xrtGetError());
            assert(xl_script_module_unit_checked());
            return index;
        }
        assert(result == 0 && xrtGetError());
        xrtClearError();
    }
    assert(!"local construction never reached a successful allocation budget");
    return 0;
}
