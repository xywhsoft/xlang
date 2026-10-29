#include "xl6_dict_methods.h"
#include "xl6_dict.h"
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
static xvalue* finishValue(xl6_error_scope* scope,xvalue* result)
{
    if (!result || scope->failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(scope); discard(result); result=NULL;
    }
    if (!finish(scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
/* Probe identifies a family, not authority to dereference its payload. Every
 * private owner, including busy/cursor/unbound shells, enters its checked
 * native operation; only status zero can authorize the wire type check. */
static int family(const xvalue* value)
{
    if (!value) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    int native=xl6DictErasedProbeV1(value);
    if (native) return native;
    if (xrtValueType(value)==XVALUE_OBJECT) return 0;
    xrtSetErrorKind(XERR_TYPE); return -1;
}
static bool count(const xvalue* source,bool capacity,int64_t* output)
{
    int native=family(source); if (native<0) return false;
    if (native) return xl6DictErasedCountV1(source,capacity,output);
    size_t result=capacity ? xrtValueCapacity(source) : xrtValueCount(source);
    if (result>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (xrtGetError()) return false;
    *output=(int64_t)result; return true;
}
static int64_t measure(const xvalue* source,bool capacity)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return 0;
    int64_t result=0; bool success=count(source,capacity,&result);
    return finish(&scope,success) ? result : 0;
}
int64_t xl6DictValueCountV1(const xvalue* source) { return measure(source,false); }
int64_t xl6DictValueCapacityV1(const xvalue* source) { return measure(source,true); }
bool xl6DictValueCountIfPresentV1(const xvalue* source,int64_t* output)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    bool present=source && xrtValueType(source)!=XVALUE_NULL;
    int64_t result=0; bool success=output && (!present || count(source,false,&result));
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    if (!finish(&scope,success)) return false;
    if (present) *output=result;
    return present;
}
xvalue* xl6DictValueCountOrNullV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    int64_t size=0; xvalue* result=NULL;
    if (!source || xrtValueType(source)==XVALUE_NULL) result=xrtValueRetain(xrtValueNull());
    else if (count(source,false,&size)) result=xrtValueInt(size);
    return finishValue(&scope,result);
}
static bool key(const xvalue* source,xstrview text,bool remove,bool* found)
{
    if (text.Size && !text.Data) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    int native=family(source); if (native<0) return false;
    if (native) return xl6DictErasedKeyV1(source,text,remove,found);
    xvalue* receiver=xrtValueRetain(source); xvalue* retired=NULL; bool success=false;
    if (receiver) {
        const xvalue* old=xrtValueObjectGet(receiver,text); *found=old!=NULL;
        if (remove && old) retired=xrtValueRetain(old);
        if (!xrtGetError() && (!remove || !old || retired))
            success=!remove || !old || xrtValueObjectRemove(receiver,text);
    }
    if (!success || xrtGetError())
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(xrtGetError())});
    discard(retired); discard(receiver); return success;
}
bool xl6DictValueHasV1(const xvalue* source,const xvalue* item)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    bool found=false,success=false;
    if (!item) xrtSetErrorKind(XERR_ARGUMENT);
    else if (xrtValueType(item)!=XVALUE_STRING) xrtSetErrorKind(XERR_TYPE);
    else {
        xstrview text={0};
        success=xrtValueGetString(item,&text) && key(source,text,false,&found);
    }
    return finish(&scope,success) && found;
}
bool xl6DictValueRemoveV1(const xvalue* source,xstrview text)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    bool found=false,success=key(source,text,true,&found);
    return finish(&scope,success) && found;
}
bool xl6DictValueClearV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    int native=family(source); bool success=false;
    if (native>0) success=xl6DictErasedClearV1(source);
    else if (!native) {
        /* XRT's owned snapshot pins all retired children until Clear leaves
         * BUSY. End and receiver retirement happen outside that exclusion. */
        xvalue* receiver=xrtValueRetain(source); xvalueiter snapshot={0};
        bool started=receiver && xrtValueIterBegin(receiver,&snapshot);
        if (started) success=xrtValueClear(receiver);
        if (!success || scope.failed || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
        if (started) xrtValueIterEnd(&snapshot);
        if (scope.failed || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
        discard(receiver);
    }
    return finish(&scope,success);
}
bool xl6DictValueReserveV1(const xvalue* source,int64_t size)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    int native=family(source); bool success=false;
    if (size<0 || (uint64_t)size>SIZE_MAX) xrtSetErrorKind(XERR_RANGE);
    else if (native>0) success=xl6DictErasedReserveV1(source,size);
    else if (!native) success=xrtValueReserve((xvalue*)source,(size_t)size);
    return finish(&scope,success);
}
bool xl6DictValueTrimV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    int native=family(source);
    bool success=native>0 ? xl6DictErasedTrimV1(source) : !native && xrtValueTrim((xvalue*)source);
    return finish(&scope,success);
}
xvalue* xl6DictValueTakeV1(const xvalue* source,xstrview text)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    xvalue* result=NULL; bool found=false,success=false;
    if (text.Size && !text.Data) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        int native=family(source);
        if (native>0) success=xl6DictErasedReadV1(source,text,true,&result,&found);
        else if (!native) {
            xvalue* receiver=xrtValueRetain(source); xvalue* retired=NULL;
            if (receiver) {
                const xvalue* old=xrtValueObjectGet(receiver,text); found=old!=NULL;
                if (old) retired=xrtValueRetain(old);
                if (!xrtGetError() && (!old || retired)) {
                    result=old ? xrtValueObjectTake(receiver,text) : NULL;
                    success=!old || result!=NULL;
                }
            }
            if (!success || scope.failed || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
            discard(retired); discard(receiver);
        }
    }
    if (success && !found && !scope.failed && !xrtGetError()) result=xrtValueRetain(xrtValueNull());
    return finishValue(&scope,result);
}
static xvalue* clone(const xvalue* source)
{
    int native=family(source);
    return native>0 ? xl6DictErasedCloneV1(source) : !native ? xrtValueClone(source) : NULL;
}
xvalue* xl6DictValueCloneV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    return finishValue(&scope,clone(source));
}
static bool merge(xvalue* target,const xvalue* source)
{
    int left=family(target),right=family(source);
    if (left<0 || right<0) return false;
    if (left && right) {
        XL6_DictLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
        bool success=xl6DictSchemaCopyV1(target,&layout,&policy) &&
            xl6DictMergeV1(target,source,&layout,true);
        if (!success || xrtGetError())
            xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(xrtGetError())});
        xl6SequenceAdmissionDropV3(&policy); return success;
    }
    if (!left) {
        xvalue* wire=right ? xl6DictToWireV1(source) : NULL;
        bool success=(!right || wire) && xrtValueObjectMerge(target,right ? wire : source,XVALUE_MERGE_REPLACE);
        if (!success || xrtGetError())
            xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(xrtGetError())});
        discard(wire); return success;
    }
    /* Native left, explicit wire right. Only a private clone is modified,
     * so an admission/copy failure cannot publish a partial merge. */
    xvalueiter iterator={0}; bool started=xrtValueIterBegin(source,&iterator),success=false;
    if (started) for (;;) {
        xvaluekey text={0}; xvalue* item=NULL;
        xvalueiterresult status=xrtValueIterAdvance(&iterator,&text,&item);
        if (xrtGetError()) break;
        if (status==XVALUE_ITER_END) { success=true; break; }
        if (status!=XVALUE_ITER_ITEM) break;
        if (text.Type!=XVALUE_KEY_STRING) { xrtSetErrorKind(XERR_TYPE); break; }
        if (!xl6DictErasedSetV1(target,text.String,item)) break;
    }
    if (!success || xrtGetError())
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(xrtGetError())});
    if (started) xrtValueIterEnd(&iterator);
    return success;
}
xvalue* xl6DictValueMergeRightV1(const xvalue* left,const xvalue* right)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    xvalue* result=clone(left);
    if (!result || !merge(result,right) || scope.failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(&scope); discard(result); result=NULL;
    }
    return finishValue(&scope,result);
}
xvalue* xl6DictValueWithoutKeyV1(const xvalue* left,xstrview text)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    xvalue* result=clone(left); bool found=false;
    if (!result || !key(result,text,true,&found) || scope.failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(&scope); discard(result); result=NULL;
    }
    return finishValue(&scope,result);
}
