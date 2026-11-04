#include "src/common/xl6_xrt.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_callback_failure.h"
#include "src/runtime/xl6_foreign_callback_scope.h"
#include <assert.h>
#include <stdio.h>

static void counts(uint64_t expectedPending,uint64_t expectedTotal)
{uint64_t p=0,t=0;assert(xl6CallbackFailureCountV1(&p,&t));assert(p==expectedPending && t==expectedTotal);}
static int32 producer(void* ignored)
{
    (void)ignored;
    for(unsigned i=0;i<10000;++i)xl6CallbackFailureRecordV1((xl6_error_result){false,NULL});
    return 0;
}
static xl6_error_result boundary(void)
{
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    return xl6ErrorScopeEndAtSafePointV1(&scope);
}
int main(void)
{
    assert(!xl6ErrorScopeCanAcceptDeferredFailureV1()); /* Also valid before a first language boundary. */
    assert(boundary().ok); /* Warm the independent checked-context metadata. */
    XL6_ForeignCallbackScopeV1 callback={0};
    assert(xl6ForeignCallbackEnterV1(&callback));assert(xl6ForeignCallbackLeaveV1(&callback));
    xl6_error_result output={false,(xerror*)(uintptr_t)1};uint64_t count=99;
    assert(xl6CallbackFailureTakeV1(&output,&count) && output.ok && !output.error && count==0);
    xerror* first=xrtErrorCreate(XERR_STATE,"test",1,"first callback");assert(first);
    xerror* later=xrtErrorCreate(XERR_STATE,"test",2,"later callback");assert(later);
    xl6CallbackFailureRecordV1((xl6_error_result){false,first});
    xl6CallbackFailureRecordV1((xl6_error_result){false,later});counts(2,2);
    assert(xl6CallbackFailureTakeV1(&output,&count));assert(!output.ok && output.error==first && count==2);
    xrtErrorFree(output.error);counts(0,2);assert(boundary().ok);

    /* Failed OOM carriers remain failures, and mailbox work cannot allocate. */
    assert(xrtMemDebugFailAfter(0));
    xl6CallbackFailureRecordV1((xl6_error_result){false,NULL});counts(1,3);
    assert(xl6CallbackFailureTakeV1(&output,&count) && !output.ok && !output.error && count==1);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();assert(boundary().ok);

    /* A primary boundary failure must not acknowledge a pending batch. */
    first=xrtErrorCreate(XERR_STATE,"test",3,"deferred");assert(first);
    xl6CallbackFailureRecordV1((xl6_error_result){false,first});
    xl6_error_scope failed={0};assert(xl6ErrorScopeBegin(&failed));
    xl6ErrorScopeCaptureFailure(&failed); /* Explicit failure with NULL diagnostic. */
    output=xl6ErrorScopeEndAtSafePointV1(&failed);assert(!output.ok && !output.error);counts(1,4);
    output=boundary();assert(!output.ok && output.error==first);xrtErrorFree(output.error);counts(0,4);

    /* Postpone on this active callback context; other safe threads may drain.
     * Native snapshot references independently protect physical return tails. */
    assert(xl6ForeignCallbackEnterV1(&callback));
    xl6CallbackFailureRecordV1((xl6_error_result){false,NULL});
    assert(boundary().ok);counts(1,5);
    assert(xl6ForeignCallbackLeaveV1(&callback));
    output=boundary();assert(!output.ok && !output.error);counts(0,5);

    /* Racing producers share one first-error/total transaction. */
    xthread* threads[4];
    for(unsigned i=0;i<4;++i){threads[i]=xrtThreadCreate(producer,NULL,0);assert(threads[i]);}
    for(unsigned i=0;i<4;++i){assert(xrtThreadWait(threads[i])==XWAIT_OK);xrtThreadDestroy(threads[i]);}
    counts(40000,40005);assert(xl6CallbackFailureTakeV1(&output,&count));
    assert(!output.ok && !output.error && count==40000);counts(0,40005);assert(boundary().ok);

    count=77;output=(xl6_error_result){true,NULL};
    assert(!xl6CallbackFailureTakeV1(NULL,&count) && count==77);xrtClearError();
    assert(!xl6CallbackFailureCountV1(&count,&count) && count==77);xrtClearError();
    assert(!xl6CallbackFailureTakeV1(&output,(uint64_t*)&output));assert(output.ok && !output.error);xrtClearError();
    puts("resident callback failure mailbox: cold query, first error, OOM, deferred retry, context gate and 40000 concurrent events passed");
    return 0;
}
