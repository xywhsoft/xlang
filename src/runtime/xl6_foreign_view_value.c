#include "src/common/xl6_xrt.h"
#include "xl6_foreign_view_value_storage.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

static bool clone_view(void* raw,void** output,void* user)
{
    if(!raw || !output || user){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ForeignView* copy=xl6ForeignViewRetainV1(raw);
    if(!copy)return false;
    *output=copy;return true;
}
static void drop_view(void* raw,void* user)
{
    if(user)abort();
    XL6_ForeignCallbackScopeV1 callback={0};
    if(!xl6ForeignCallbackEnterV1(&callback))abort();
    if(!xl6ForeignViewReleaseV1(raw))xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    if(!xl6ForeignCallbackLeaveV1(&callback))abort();
}
static const xvaluehandleops operations={clone_view,drop_view,NULL,NULL};
static bool trace_view(const xvalue* value,xrtownershipvisitor visit,void* user)
{
    void* raw=NULL;void* context=NULL;const xvaluehandleops* actual=NULL;
    if(!visit || !xrtValueGetHandle(value,&raw,&actual,&context) || actual!=&operations || !raw || context)return false;
    return visit(xl6ForeignViewOwnershipV1(raw),user);
}
static const xrtownershipadapterv1* adapter(xrtownershipref ref)
{
    const xrtownershipadapterv1* result=xrtValueHandleOwnershipAdapterV1(ref,&operations,trace_view);
    return result && xrtValueTypeId(ref.Data) ? result : NULL;
}
static bool finish(xl6_error_scope* errors,bool success)
{
    if(!success || xrtGetError() || errors->failed)xl6ErrorScopeCaptureFailure(errors);
    xl6_error_result result=xl6ErrorScopeEnd(errors);
    if(!result.ok)xrtSetErrorTake(result.error);
    return result.ok;
}
static xvalue* box(XL6_ForeignView* view,uint64_t identity)
{
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return NULL;
    xvalue* value=NULL;XL6_ForeignView* copy=NULL;
    if(!view)value=xrtValueNull();
    else if((copy=xl6ForeignViewRetainV1(view))!=NULL){
        void* slot=copy;
        value=xrtValueHandleTake(&slot,&operations,NULL);copy=slot;
        if(value && (!xrtValueHandleOwnershipBindPhased(value,trace_view) ||
            (identity && !xrtValueTypeIdBind(value,identity)))){
            xl6ErrorScopeCaptureFailure(&errors);xrtValueRelease(value);value=NULL;
        }
    }
    if(!value || xrtGetError() || errors.failed)xl6ErrorScopeCaptureFailure(&errors);
    if(copy && !xl6ForeignViewReleaseV1(copy))xl6ErrorScopeCaptureFailure(&errors);
    if(errors.failed){xrtValueRelease(value);value=NULL;}
    return finish(&errors,value!=NULL)?value:NULL;
}
xvalue* xl6ForeignViewBoxV1(XL6_ForeignView* view,uint64_t identity)
{
    if(!identity){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return box(view,identity);
}
xvalue* xl6ForeignViewStorageBoxV1(XL6_ForeignView* view){return box(view,0);}
bool xl6ForeignViewValueAdmitsV1(const xvalue* value,uint64_t identity,bool nullable)
{
    if(!identity)return false;
    if(!value || xrtValueType(value)==XVALUE_NULL)return nullable;
    return xrtValueTypeId(value)==identity && adapter(xrtValueOwnership(value));
}
bool xl6ForeignViewStorageUnboxV1(const xvalue* value,XL6_ForeignView** output)
{
    if(!output || *output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!value || xrtValueType(value)==XVALUE_NULL)return true;
    if(!adapter(xrtValueOwnership(value))){xrtSetErrorKind(XERR_TYPE);return false;}
    void* raw=NULL;void* context=NULL;const xvaluehandleops* actual=NULL;
    if(!xrtValueGetHandle(value,&raw,&actual,&context)){xrtSetErrorKind(XERR_TYPE);return false;}
    XL6_ForeignView* copy=xl6ForeignViewRetainV1(raw);
    if(!copy)return false;
    *output=copy;return true;
}
bool xl6ForeignViewUnboxV1(const xvalue* value,uint64_t identity,bool nullable,XL6_ForeignView** output)
{
    if(!output || (uintptr_t)output % _Alignof(XL6_ForeignView*) || *output || !identity){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return false;
    XL6_ForeignView* copy=NULL;bool success=false;
    if(!value || xrtValueType(value)==XVALUE_NULL){
        if(nullable)success=true;else xrtSetErrorKind(XERR_TYPE);
    }else if(!adapter(xrtValueOwnership(value)) || xrtValueTypeId(value)!=identity)xrtSetErrorKind(XERR_TYPE);
    else {
        void* raw=NULL;void* context=NULL;const xvaluehandleops* actual=NULL;
        if(xrtValueGetHandle(value,&raw,&actual,&context)){
            copy=xl6ForeignViewRetainV1(raw);success=copy!=NULL;
        }
    }
    if(!success || xrtGetError() || errors.failed){
        xl6ErrorScopeCaptureFailure(&errors);
        if(copy && !xl6ForeignViewReleaseV1(copy))xl6ErrorScopeCaptureFailure(&errors);
        copy=NULL;success=false;
    }
    if(!finish(&errors,success))return false;
    *output=copy;return true;
}
bool xl6ForeignViewTryUnboxV1(const xvalue* value,uint64_t identity,XL6_ForeignView** output)
{
    if(!output || (uintptr_t)output % _Alignof(XL6_ForeignView*) || *output || !identity){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ForeignViewValueAdmitsV1(value,identity,true))return true;
    return xl6ForeignViewUnboxV1(value,identity,true,output);
}
static bool resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** output,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    (void)policies;(void)preparation;(void)completion;
    const xrtownershipadapterv1* result=adapter(ref);
    if(!result)return false;
    *output=result;return true;
}
const XL6_OwnershipDomainV1* xl6OwnershipDomainForeignViewValueV1Get(void)
{static const XL6_OwnershipDomainV1 domain={sizeof(domain),NULL,resolve,NULL};return &domain;}
