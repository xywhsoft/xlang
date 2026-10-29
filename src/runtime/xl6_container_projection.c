#include "src/runtime/xl6_container_projection.h"
#include "src/api/xl_foreign_error.h"

bool xl6ContainerProjectionWitnessTakeV1(xvalue* witness,const xvalue* source,bool nullable,
    void* schema,const XL6_ContainerProjectionOpsV1* ops,xvalue** output)
{
    xl6_error_scope scope={0}; xvalue* result=NULL; bool captured=false;
    if(!xl6ErrorScopeBegin(&scope)) {
        xerror* first=xrtTakeError(); xrtValueRelease(witness);
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,first});
        return false;
    }
    bool valid=schema && ops && ops->Capture && ops->Match && ops->Drop;
    if(!valid || !output || *output || !source || !witness || witness==source)
        xrtSetErrorKind(XERR_ARGUMENT);
    else captured=ops->Capture(schema,witness);
    if(!captured || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(witness);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    if(captured && !scope.failed) {
        bool match=ops->Match(schema,source);
        if(xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
        else if(match)result=xrtValueRetain(source);
        else if(nullable)result=xrtValueRetain(xrtValueNull());
        else xrtSetErrorKind(XERR_TYPE);
    }
    if(!result || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    /* Drop is a cleanup step, not a new operation that may erase an earlier
     * failure. The independent bit preserves a first NULL diagnostic. */
    if(valid) {
        xl6_error_scope cleanup={0};
        if(xl6ErrorScopeBeginCleanup(&cleanup,false)) {
            ops->Drop(schema);
            if(xrtGetError())xl6ErrorScopeCaptureFailure(&cleanup);
            xl6_error_result dropped=xl6ErrorScopeEnd(&cleanup);
            if(!dropped.ok)xl6ErrorScopePropagateFailure(dropped);
        } else {
            xl6ErrorScopeCaptureFailure(&scope); ops->Drop(schema);
        }
    }
    if(scope.failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
        if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok) {
        xrtValueRelease(result); xrtClearError(); xl6ErrorScopePropagateFailure(outcome);
        return false;
    }
    *output=result; return true;
}
