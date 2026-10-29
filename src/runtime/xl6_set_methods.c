#include "xl6_set_methods.h"
#include "src/api/xl_foreign_error.h"

static bool enter(xl6_error_scope* scope)
{
    if (xl6ErrorScopeBegin(scope)) return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
}
static bool finish(xl6_error_scope* scope,bool success)
{
    if (!success || scope->failed || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
/* Even a refused error-entry/cleanup-entry must consume the owned argument.
 * Preserve and propagate the first failure before a destructor can report a
 * second one. No abort or leaked ownership when diagnostics cannot allocate. */
static void discard(xvalue* value)
{
    if (!value) return;
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,false)) {
        xrtErrorFree(incoming); xrtValueRelease(value);
        xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
        if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    } else {
        xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(incoming)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(incoming);
    }
}
bool xl6SetValueAddTakeV1(const xvalue* source,xvalue* item)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) { discard(item); return false; }
    bool added=false;
    bool success=xl6SetErasedKeyV1(source,item,XL6_SET_ADD_V1,&added);
    if (!success || scope.failed || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    discard(item);
    return finish(&scope,success);
}
static bool key(const xvalue* source,const xvalue* item,XL6_SetKeyActionV1 action)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    bool found=false; bool success=xl6SetErasedKeyV1(source,item,action,&found);
    return finish(&scope,success) && found;
}
bool xl6SetValueHasV1(const xvalue* source,const xvalue* item)
{ return key(source,item,XL6_SET_HAS_V1); }
bool xl6SetValueRemoveV1(const xvalue* source,const xvalue* item)
{ return key(source,item,XL6_SET_REMOVE_V1); }
bool xl6SetValueClearV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    return finish(&scope,xl6SetErasedClearV1(source));
}
bool xl6SetValueReserveV1(const xvalue* source,int64_t size)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    return finish(&scope,xl6SetErasedReserveV1(source,size));
}
bool xl6SetValueTrimV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    return finish(&scope,xl6SetErasedTrimV1(source));
}
static int64_t count(const xvalue* source,bool capacity)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return 0;
    int64_t result=0; bool success=xl6SetErasedCountV1(source,capacity,&result);
    return finish(&scope,success) ? result : 0;
}
int64_t xl6SetValueCountV1(const xvalue* source) { return count(source,false); }
int64_t xl6SetValueCapacityV1(const xvalue* source) { return count(source,true); }
bool xl6SetValueCountIfPresentV1(const xvalue* source,int64_t* output)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    bool present=source && xrtValueType(source)!=XVALUE_NULL;
    int64_t result=0; bool success=output && (!present || xl6SetErasedCountV1(source,false,&result));
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    if (!finish(&scope,success)) return false;
    if (present) *output=result;
    return present;
}
static xvalue* finishValue(xl6_error_scope* scope,xvalue* result)
{
    if (!result || scope->failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(scope); discard(result); result=NULL;
    }
    /* Value producers have a NULL failure carrier. Do not use the scoped
     * payload-only boolean/count finisher: its callback notification would
     * bypass a language catch on this ordinary checked return. */
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xrtSetErrorTake(outcome.error);
    return result;
}
xvalue* xl6SetValueCountOrNullV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    xvalue* result=NULL; int64_t size=0;
    if (!source || xrtValueType(source)==XVALUE_NULL) result=xrtValueRetain(xrtValueNull());
    else if (xl6SetErasedCountV1(source,false,&size)) result=xrtValueInt(size);
    return finishValue(&scope,result);
}
xvalue* xl6SetValueCloneV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    return finishValue(&scope,xl6SetErasedCloneV1(source));
}
xvalue* xl6SetValueAlgebraV1(const xvalue* left,const xvalue* right,unsigned int operation)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    return finishValue(&scope,xl6SetErasedAlgebraV1(left,right,(XL6_SetAlgebraV1)operation));
}
bool xl6SetValueRelationV1(const xvalue* left,const xvalue* right,unsigned int operation,bool proper)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    bool result=false; bool success=xl6SetErasedRelationV1(left,right,(XL6_SetRelationV1)operation,proper,&result);
    return finish(&scope,success) && result;
}
