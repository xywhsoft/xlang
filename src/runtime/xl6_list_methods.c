#include "src/runtime/xl6_list_methods.h"
#include "src/runtime/xl6_list.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>

/* A family match is NOT admission. Native operations perform the complete
 * private-owner/projection pin. Unknown handles are never payload-cast. */
static int carrier(const xvalue* source)
{
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    int native=xl6ListErasedProbeV1(source);
    if (native) return native;
    if (xrtValueType(source)==XVALUE_INT_MAP) return 0;
    xrtSetErrorKind(XERR_TYPE); return -1;
}
static bool finish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
static bool count(const xvalue* source,int64_t* output)
{
    int kind=carrier(source);
    if (kind<0) return false;
    if (kind) return xl6ListErasedCountV1(source,output);
    size_t size=xrtValueCount(source);
    if (size>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    *output=(int64_t)size; return true;
}
int64_t xl6ListValueCountV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return 0;
    }
    int64_t result=0; bool success=count(source,&result);
    return finish(&scope,success) ? result : 0;
}
bool xl6ListValueCountIfPresentV1(const xvalue* source,int64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    int64_t result=0; bool present=source && xrtValueType(source)!=XVALUE_NULL;
    bool success=output && (!present || count(source,&result));
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    if (!finish(&scope,success)) return false;
    if (present) *output=result;
    return present;
}
xvalue* xl6ListValueCountOrNullV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    int64_t size=0; xvalue* result=NULL;
    if (!source || xrtValueType(source)==XVALUE_NULL) result=xrtValueRetain(xrtValueNull());
    else if (count(source,&size)) result=xrtValueInt(size);
    if (!result || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    return finish(&scope,result!=NULL) ? result : NULL;
}
bool xl6ListValueRemoveV1(xvalue* source,int64_t key)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    int kind=carrier(source); bool found=false,success=kind>=0;
    if (kind>0) success=xl6ListErasedKeyV1(source,key,true,&found);
    else if (!kind) found=xrtValueIntMapRemove(source,key);
    return finish(&scope,success) && found;
}
bool xl6ListValueClearV1(xvalue* source)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    int kind=carrier(source);
    bool success=kind>0 ? xl6ListErasedClearV1(source) : kind==0 && xrtValueClear(source);
    return finish(&scope,success);
}
int64_t xl6ListValueTrimV1(xvalue* source,int64_t retainEmpty)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return -1;
    }
    int kind=carrier(source); int64_t result=0;
    bool success=kind>=0 && retainEmpty>=0 && (uint64_t)retainEmpty<=SIZE_MAX;
    if (kind>=0 && !success) xrtSetErrorKind(XERR_RANGE);
    if (success && kind) success=xl6ListErasedTrimV1(source,retainEmpty,&result);
    else if (success) {
        size_t released=xrtValueIntMapTrim(source,(size_t)retainEmpty);
        if (released>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); success=false; }
        else result=(int64_t)released;
    }
    return finish(&scope,success) ? result : -1;
}
xvalue* xl6ListValueTakeV1(xvalue* source,int64_t key)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    int kind=carrier(source); xvalue* result=NULL; bool found=false,success=kind>=0;
    if (kind>0) success=xl6ListErasedReadV1(source,key,true,&result,&found);
    else if (!kind) { result=xrtValueIntMapTake(source,key); found=result!=NULL; }
    if (success && !found && !xrtGetError() && !scope.failed) result=xrtValueRetain(xrtValueNull());
    if (!success || !result || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    return finish(&scope,result!=NULL) ? result : NULL;
}
bool xl6ListValueContainsV1(const xvalue* source,const xvalue* item)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    int kind=carrier(source); bool found=false,success=kind>=0 && item;
    xvalueiter* iterator=NULL;
    if (!item) xrtSetErrorKind(XERR_ARGUMENT);
    if (success && kind) success=xl6ListErasedContainsV1(source,item,&found);
    else if (success) {
        iterator=xrtValueIterCreate(source); success=iterator!=NULL;
        while (success && !found && !scope.failed) {
            const xvalue* current=xrtValueIterNext(iterator,NULL);
            if (xrtGetError()) { success=false; break; }
            if (!current) break;
            found=xrtValueEqual(current,item);
            if (xrtGetError()) success=false;
        }
    }
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueIterDestroy(iterator);
    return finish(&scope,success) && found;
}
