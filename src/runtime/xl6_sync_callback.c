#include "src/common/xl6_xrt.h"
#include "xl6_sync_callback.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

bool xl6SyncCallbackBeginV1(XL6_SyncCallbackContextV1* context,const XL6_CodeOwner* owner,XL6_Closure* closure,uint64_t signature)
{
    if(!context || (uintptr_t)context%_Alignof(XL6_SyncCallbackContextV1) || context->self || context->boundary.self || context->boundary.previous || context->boundary.thread ||
        context->activity.self || context->activity.identity.size || context->activity.identity.context ||
        context->activity.identity.acquire || context->activity.identity.release || context->activity.previous || context->activity.next || context->activity.threadId ||
        context->closure.closure || context->closure.code.lease || context->closure.code.previous ||
        context->closure.code.next || context->closure.code.threadId || context->active || context->signature || context->firstError || context->completed ||
        xrtAtomic32Load(&context->failed,XMEMORY_RELAXED) || !closure || !signature ||
        !xl6ClosureOwnershipAdapterV1(xl6ClosureRuntimeV1Get()->Ownership(closure))){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(!xl6ForeignCallbackEnterV1(&context->boundary))return false;
    xl6CodeActivityEnterV1(&context->activity,owner);
    xl6ClosureRuntimeV1Get()->Enter(&context->closure,closure);
    context->signature=signature;context->self=context;return true;
}
bool xl6SyncCallbackEnterV1(XL6_SyncCallbackContextV1* context,uint64_t signature,XL6_SyncCallbackScopeV1* scope)
{
    if(!context || context->self!=context || context->signature!=signature || !scope || scope->self || scope->context ||
        scope->previous || scope->closure){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(!xl6ForeignCallbackContainsV1(&context->boundary)){
        xrtSetErrorKind(XERR_STATE);return false;
    }
    if(context->completed){xrtSetErrorKind(XERR_STATE);return false;}
    /* Failure state may be published by a rejected worker. Observe its flag; never enter captured language state after the first failure. */
    bool failed=xrtAtomic32Load(&context->failed,XMEMORY_ACQUIRE)!=0;
    if(failed){xrtSetErrorKind(XERR_STATE);return false;}
    scope->previous=context->active;scope->closure=context->closure.closure;
    scope->context=context;scope->self=scope;context->active=scope;return true;
}
xl6_error_result xl6SyncCallbackLeaveV1(XL6_SyncCallbackScopeV1* scope,xl6_error_result incoming)
{
    if(!scope || scope->self!=scope || !scope->context || scope->context->active!=scope ||
        scope->closure!=scope->context->closure.closure || !xl6ForeignCallbackContainsV1(&scope->context->boundary))abort();
    scope->context->active=scope->previous;
    *scope=(XL6_SyncCallbackScopeV1){0};return incoming;
}
void xl6SyncCallbackReportV1(XL6_SyncCallbackContextV1* context,xl6_error_result incoming)
{
    if(incoming.ok || !context || context->self!=context)abort();
    uint32 expected=0;
    if(xrtAtomic32CompareExchange(&context->failed,&expected,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){
        context->firstError=incoming.error;
        /* Valid synchronous calls share the caller's checked transaction.
         * Publish immediately so errors from different callback parameters or
         * retained callbacks keep their observed order. Rejected worker/fiber
         * attempts stage their owned error until the native caller joins them. */
        if(xl6ForeignCallbackContainsV1(&context->boundary)){
            xl6_error_result published={false,xrtErrorRef(incoming.error)};
            if(!xl6ErrorScopeTryPropagateFailureV1(&published))xrtErrorFree(published.error);
        }
    }
    else xrtErrorFree(incoming.error);
}
xl6_error_result xl6SyncCallbackCompleteV1(XL6_SyncCallbackContextV1* context)
{
    if(!context || context->self!=context || context->active || context->completed ||
        !xl6ForeignCallbackContainsV1(&context->boundary))abort();
    context->completed=true;
    xl6_error_result result={xrtAtomic32Exchange(&context->failed,0,XMEMORY_ACQ_REL)==0,context->firstError};
    context->firstError=NULL;return result;
}
xl6_error_result xl6SyncCallbackEndV1(XL6_SyncCallbackContextV1* context)
{
    if(!context || context->self!=context || context->active || !xl6ForeignCallbackContainsV1(&context->boundary))abort();
    xl6_error_result incoming=context->completed ? (xl6_error_result){true,NULL} : xl6SyncCallbackCompleteV1(context);
    /* Native has joined every attempt. An already latched outer checked
     * failure stays primary; otherwise the first reported callback outcome
     * precedes the native diagnostic and later cleanup, including NULL error.
     * Begin keeps the native diagnostic separate until the caller consumes
     * this result. Never replace a known failure with a later allocated one. */
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))abort();
    if(errors.previous && errors.previous->failed){
        errors.failed=true;errors.pending=xrtErrorRef(errors.previous->pending);
    }
    if(!incoming.ok){xrtSetErrorTake(incoming.error);xl6ErrorScopeCaptureFailure(&errors);}
    else if(errors.previous_diagnostic){
        xrtSetErrorTake(xrtErrorRef(errors.previous_diagnostic));xl6ErrorScopeCaptureFailure(&errors);
    }
    xl6ClosureRuntimeV1Get()->Leave(&context->closure);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    /* A native adopter/cleanup that violates the nonescaping contract may
     * still attempt a callback while this stack frame exists. Enter rejects
     * completed contexts; consume that reported failure without leaking its
     * diagnostic. This does not make userdata safe after End or without joins. */
    if(xrtAtomic32Exchange(&context->failed,0,XMEMORY_ACQ_REL)){
        xrtSetErrorTake(context->firstError);context->firstError=NULL;xl6ErrorScopeCaptureFailure(&errors);
    }
    xl6CodeActivityLeaveV1(&context->activity);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    if(!xl6ForeignCallbackLeaveV1(&context->boundary))abort();
    *context=(XL6_SyncCallbackContextV1){0};return xl6ErrorScopeEnd(&errors);
}
