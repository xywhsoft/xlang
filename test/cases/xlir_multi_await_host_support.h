#ifndef XLIR_MULTI_AWAIT_HOST_SUPPORT_H
#define XLIR_MULTI_AWAIT_HOST_SUPPORT_H
#include "xruntime_decl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#undef xl6ContinuationWatchRegister
#undef xl6ContinuationCommit
typedef enum XLIR_TestCompletion {
    XLIR_TEST_RESOLVE, XLIR_TEST_REJECT, XLIR_TEST_CANCEL, XLIR_TEST_CLOSE
} XLIR_TestCompletion;
static bool xlir_multi_await_complete(xpromise* promise,void* data,XLIR_TestCompletion mode)
{
    bool ok=true;
    if (mode==XLIR_TEST_RESOLVE) ok=xrtPromiseResolve(promise,data);
    else if (mode==XLIR_TEST_REJECT) {
        xerror* error=xrtErrorCreate(XERR_PROTOCOL,"xlir-test",1,"expected chain failure");
        assert(error); ok=xrtPromiseReject(promise,error); xrtErrorFree(error);
    } else if (mode==XLIR_TEST_CANCEL) ok=xrtPromiseCancel(promise);
    xrtPromiseDestroy(promise); return ok;
}
#ifdef XLIR_MULTI_AWAIT_PENDING
typedef struct XLIR_TestPending {
    xfuture* future; xpromise* promise; void* data; XLIR_TestCompletion mode;
} XLIR_TestPending;
static XLIR_TestPending xlir_multi_await_pending[3];
static size_t xlir_multi_await_pending_count;
static xatomic32 xlir_multi_await_pending_registrations;
#ifdef XLIR_MULTI_AWAIT_AFTER_COMMIT
/* Watch and its handoff Commit occur on the same activation thread. TLS avoids
 * polling/racing mutable entries belonging to another activation worker. */
static _Thread_local XLIR_TestPending* xlir_multi_await_commit_pending;
static _Thread_local XL6_ContinuationTicket* xlir_multi_await_commit_ticket;
static void xlir_multi_await_pending_commit(XL6_ContinuationTicket* ticket)
{
    XLIR_TestPending* pending=xlir_multi_await_commit_ticket==ticket ? xlir_multi_await_commit_pending : NULL;
    if (pending) assert(pending->promise && xrtFutureState(pending->future)==XFUTURE_PENDING);
    xl6ContinuationCommit(ticket);
    /* The ticket may be retired by completion. Never dereference it again. */
    if (pending) {
        xlir_multi_await_commit_pending=NULL; xlir_multi_await_commit_ticket=NULL;
        xpromise* promise=pending->promise; pending->promise=NULL;
        assert(xlir_multi_await_complete(promise,pending->data,pending->mode));
    }
}
#endif
static bool xlir_multi_await_pending_register(XL6_ContinuationTicket* ticket,xfuturewatch* watch,xfuture* source)
{
    XLIR_TestPending* pending=NULL;
    /* Entries/future identities are published before the initial activation;
     * each Promise belongs to exactly one serial observation of this fixture.
     * Do not read mutable Promise fields in entries belonging to other workers. */
    for (size_t i=0;i<xlir_multi_await_pending_count;++i)
        if (xlir_multi_await_pending[i].future==source) pending=&xlir_multi_await_pending[i];
    assert(pending && pending->promise && xrtFutureState(source)==XFUTURE_PENDING);
    if (!xl6ContinuationWatchRegister(ticket,watch,source)) return false;
    assert(xrtFutureState(source)==XFUTURE_PENDING);
    (void)xrtAtomic32FetchAdd(&xlir_multi_await_pending_registrations,1,XMEMORY_RELAXED);
#ifdef XLIR_MULTI_AWAIT_AFTER_COMMIT
    assert(!xlir_multi_await_commit_pending);
    xlir_multi_await_commit_pending=pending; xlir_multi_await_commit_ticket=ticket;
#else
    xpromise* promise=pending->promise; pending->promise=NULL;
    assert(xlir_multi_await_complete(promise,pending->data,pending->mode));
#endif
    return true;
}
#endif
static xfuture* xlir_multi_await_future(void* data,XLIR_TestCompletion mode)
{
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL);
    assert(promise && future);
#ifdef XLIR_MULTI_AWAIT_PENDING
    assert(xlir_multi_await_pending_count<3);
    xlir_multi_await_pending[xlir_multi_await_pending_count++]=(XLIR_TestPending){future,promise,data,mode};
#else
    assert(xlir_multi_await_complete(promise,data,mode));
#endif
    return future;
}

/* The generated module is in this SAME TU. Do not use a second preincluded
 * copy or infer success merely from a nonzero process exit. Finalization waits
 * for the selected executor and clears its real roots before checking balance.
 * These are generated-language/shared-runtime tests, not full runtime ASan. */
static int xlir_multi_await_check(bool success, int* calls, int expectedCalls, xerrkind kind)
{
    assert(xrtMemDebugEnable(true));
    for (unsigned iteration=0;iteration<2;++iteration) {
#ifdef XLIR_MULTI_AWAIT_PENDING
        xlir_multi_await_pending_count=0;
        xrtAtomic32Init(&xlir_multi_await_pending_registrations,0);
#endif
        xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
        *calls=0; int ok=xlir_module_init(); const xerror* error=xrtGetError();
#ifdef XLIR_MULTI_AWAIT_AFTER_INIT
        XLIR_MULTI_AWAIT_AFTER_INIT(ok);
#endif
        if (!!ok!=success || *calls!=expectedCalls || (success ? error!=NULL : error==NULL)) {
            fprintf(stderr,"multi-await outcome: expected=%d ok=%d calls=%d error=%s\n",
                (int)success,ok,*calls,error ? xrtErrorMessage(error) : "none"); return 1;
        }
        if (!success) {
            if (kind!=XERR_NONE) assert(xrtErrorKind(error)==kind);
            else assert(xrtErrorKind(error)==XERR_PROTOCOL && xrtErrorCode(error)==1 &&
                strcmp(xrtErrorDomain(error),"xlir-test")==0 && strstr(xrtErrorMessage(error),"expected")!=NULL);
        }
        xrtClearError(); xlir_result_void retired=xlir_module_fini_body();
        assert(retired.ok && !xrtGetError() && xlir_module_initialized==0 &&
            xlir_continuation_executor==NULL);
#ifdef XLIR_MULTI_AWAIT_PENDING
        assert(xlir_multi_await_pending_count==(size_t)expectedCalls &&
            xrtAtomic32Load(&xlir_multi_await_pending_registrations,XMEMORY_RELAXED)==(uint32)expectedCalls);
        for (size_t i=0;i<xlir_multi_await_pending_count;++i) assert(!xlir_multi_await_pending[i].promise);
#endif
        xrtMemDebugSnapshot(&after);
        assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes &&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
            after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount &&
            after.UseAfterFreeCount==before.UseAfterFreeCount);
    }
    puts("multi-await: exact terminal outcome, two complete language lifecycles and memory balance");
#ifdef XLIR_MULTI_AWAIT_PENDING
    puts("multi-await: every input completed after actual pending Watch registration");
#ifdef XLIR_MULTI_AWAIT_AFTER_COMMIT
    puts("multi-await: deferred completion after actual activation Commit");
#endif
#endif
    return 0;
}
#endif
