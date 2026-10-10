#include "src/runtime/xl6_set_key_policy.h"
#include "src/runtime/xl6_sequence.h"
#include "src/api/xl_foreign_error.h"

/* xset's payload-only callback ABI observes the native diagnostic slot. A
 * failed checked callback can have no allocated diagnostic: first propagate
 * that exact outcome to the parent, THEN supply the allocation-free native
 * refusal. A legitimate zero hash/false equality is never a failure. */
static void callbackFailure(xl6_error_result result)
{
    xl6ErrorScopePropagateFailure(result);
    if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
}
static bool finish(xl6_error_scope* scope)
{
    if (scope->failed || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if (!result.ok) callbackFailure(result);
    return result.ok;
}
static uint64 referenceHash(const void* slot,ptr context)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        callbackFailure((xl6_error_result){false,xrtTakeError()}); return 0;
    }
    XL6_CollectionIdentityFrameV1 frame={0}; uint64 hash=0;
    if (!slot || context) xrtSetErrorKind(XERR_ARGUMENT);
    else if (xl6CollectionIdentityEnterV1(&frame)) {
        const xvalue* value=*(xvalue* const*)slot;
        if (!xrtValueHash(value ? value : xrtValueNull(),&hash)) xl6ErrorScopeCaptureFailure(&scope);
        xl6CollectionIdentityLeaveV1(&frame);
    } else xl6ErrorScopeCaptureFailure(&scope);
    return finish(&scope) ? hash : 0;
}
static bool referenceEqual(const void* left,const void* right,ptr context)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        callbackFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    XL6_CollectionIdentityFrameV1 frame={0}; bool equal=false;
    if (!left || !right || context) xrtSetErrorKind(XERR_ARGUMENT);
    else if (xl6CollectionIdentityEnterV1(&frame)) {
        equal=xl6SequenceReferenceEqualV1(*(xvalue* const*)left,*(xvalue* const*)right);
        xl6CollectionIdentityLeaveV1(&frame);
    } else xl6ErrorScopeCaptureFailure(&scope);
    return finish(&scope) && equal;
}
static uint64 futureHash(const void* slot,ptr context)
{
    if (!slot || context) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }
    uintptr_t identity=(uintptr_t)*(xfuture* const*)slot;
    return xrtHash64(&identity,sizeof(identity));
}
static bool futureEqual(const void* left,const void* right,ptr context)
{
    if (!left || !right || context) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    return *(xfuture* const*)left==*(xfuture* const*)right;
}
static const xtypedsetkeypolicy referencePolicy={referenceHash,referenceEqual,NULL};
static const xtypedsetkeypolicy futurePolicy={futureHash,futureEqual,NULL};
const xtypedsetkeypolicy* xl6SetReferenceKeyPolicyV1(void) { return &referencePolicy; }
const xtypedsetkeypolicy* xl6SetFutureKeyPolicyV1(void) { return &futurePolicy; }
