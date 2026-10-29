#include "src/runtime/xl6_list_algebra.h"
#include "src/runtime/xl6_list.h"
#include "src/api/xl_foreign_error.h"

static int carrier(const xvalue* source)
{
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    int native=xl6ListErasedProbeV1(source);
    if (native) return native;
    if (xrtValueType(source)==XVALUE_INT_MAP) return 0;
    xrtSetErrorKind(XERR_TYPE); return -1;
}
static bool enter(xl6_error_scope* scope)
{ return xl6ErrorScopeBegin(scope); }
static void capture(xl6_error_scope* scope,bool success)
{ if (!success || scope->failed || xrtGetError()) xl6ErrorScopeCaptureFailure(scope); }
static xvalue* finish(xl6_error_scope* scope,xvalue* result,bool success)
{
    capture(scope,success && result);
    if (scope->failed) { xrtValueRelease(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    /* All exported operations in this unit return an owned Value or NULL.
     * Internal callbacks were captured locally; the checked return owns the
     * outcome until its language caller chooses to handle or escape it. */
    if (!outcome.ok) xrtSetErrorTake(outcome.error);
    return result;
}
xvalue* xl6ListValueCloneV1(const xvalue* source)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    int kind=carrier(source);
    xvalue* result=kind<0 ? NULL : kind ? xl6ListErasedCloneV1(source) : xrtValueClone(source);
    return finish(&scope,result,result!=NULL);
}
/* Iterator owns a stable wire snapshot. Every slot is admitted by the actual
 * destination policy before publication into the private result. */
static bool mergeWire(xvalue* target,const xvalue* source,xl6_error_scope* scope)
{
    xvalueiter* iterator=xrtValueIterCreate(source); bool success=iterator!=NULL;
    while (success && !scope->failed && !xrtGetError()) {
        xvaluekey key={0}; const xvalue* item=xrtValueIterNext(iterator,&key);
        if (!item) { success=!scope->failed && !xrtGetError(); break; }
        if (key.Type!=XVALUE_KEY_INT) { xrtSetErrorKind(XERR_STATE); success=false; break; }
        success=xl6ListErasedSetV1(target,key.Integer,item);
    }
    capture(scope,success); xrtValueIterDestroy(iterator); capture(scope,success);
    return success && !scope->failed;
}
xvalue* xl6ListValueMergeRightV1(const xvalue* left,const xvalue* right)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    int a=carrier(left),b=a>=0 ? carrier(right) : -1;
    xvalue* result=NULL; xvalue* wire=NULL; XL6_SequenceAdmissionV3 policy={0};
    bool success=a>=0 && b>=0;
    if (success) result=xl6ListValueCloneV1(left);
    success=success && result;
    if (success && a) {
        if (b) {
            XL6_ListLayoutV1 layout={0};
            success=xl6ListSchemaCopyV1(result,&layout,&policy) && xl6ListMergeV1(result,right,&layout,true);
        } else success=mergeWire(result,right,&scope);
    } else if (success) {
        if (b) { wire=xl6ListToWireV1(right); success=wire!=NULL; }
        if (success) success=xrtValueIntMapMerge(result,b ? wire : right,XVALUE_MERGE_REPLACE);
    }
    capture(&scope,success); xl6SequenceAdmissionDropV3(&policy); capture(&scope,success);
    xrtValueRelease(wire); capture(&scope,success);
    return finish(&scope,result,success);
}
xvalue* xl6ListValueWithoutKeyV1(const xvalue* source,int64_t key)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    int kind=carrier(source); xvalue* result=kind>=0 ? xl6ListValueCloneV1(source) : NULL;
    bool success=result!=NULL,found=false;
    if (success && kind) success=xl6ListErasedKeyV1(result,key,true,&found);
    else if (success) {
        found=xrtValueIntMapHas(result,key);
        success=!xrtGetError() && (!found || xrtValueIntMapRemove(result,key));
    }
    return finish(&scope,result,success);
}
