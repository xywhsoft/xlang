#include "xl6_text_value.h"
#include "xl6_closure.h"
#include "src/api/xl_task_group_bridge.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

/* This context is borrowed synchronously by XRT Start. The group reserves
 * admission before invoke, holds no wrapper lock across it and retains no
 * closure/context/code pointer afterward. Its Future acquires the ordinary
 * native group member rights; the caller receives the original owned result. */
typedef struct StartCall {
    XL6_Closure* closure;
    bool failed;
    xerror* error;
} StartCall;
static void discard(xfuture* future)
{
    if(!future)return;
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    /* Only this returned owner slot is ours. Even on a malformed checked
     * outcome the Future may be shared; rollback grants no cancellation right. */
    xrtFutureDestroy(future);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);xrtErrorFree(outcome.error);
}
static xfuture* invoke(void* raw)
{
    StartCall* call=raw;xl6_error_scope errors={0};xfuture* result=NULL;
    if(!xl6ErrorScopeBegin(&errors)){
        call->failed=true;call->error=xrtTakeError();return NULL;
    }
    XL6_ClosureScope entered={0};const XL6_ClosureRuntimeV1* closures=xl6ClosureRuntimeV1Get();
    closures->Enter(&entered,call->closure);
    bool ok=((bool (*)(void*,xfuture**))call->closure->invoke)(call->closure->environment,&result);
    if(ok && !result && !xrtGetError())xrtSetErrorKind(XERR_STATE);
    if(!ok || !result || xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    closures->Leave(&entered);
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){
        call->failed=true;call->error=outcome.error;discard(result);return NULL;
    }
    return result;
}
xfuture* xl6TaskGroupStartLanguageV1(XL6_FutureValueProfileV1 profile,void* group,XL6_Closure* starter)
{
    if(!group || !starter || !starter->invoke || !xl6FutureValueProfileValidV1(profile)){
        xrtSetErrorKind(XERR_ARGUMENT);xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;
    }
    StartCall call={starter,false,NULL};xfuture* result=xl6TaskGroupStartV1(group,invoke,&call);
    if(call.failed){
        /* XRT may diagnose a NULL starter result. The actual language failure
         * remains primary, including its authoritative NULL diagnostic. */
        xrtClearError();discard(result);
        xl6ErrorScopePropagateFailure((xl6_error_result){false,call.error});return NULL;
    }
    if(!result)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return result;
}
