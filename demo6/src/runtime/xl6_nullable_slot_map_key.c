#include "xl6_nullable_slot_map_key.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

static bool valid(XL6_NullableSlotMapKeyV1 value)
{
    bool paired=(!value.value.instance)==(!value.value.slot);
    return paired && (value.hasValue || (!value.value.instance && !value.value.slot));
}
static bool empty(const XL6_NullableSlotMapKeyV1* out)
{return out && (uintptr_t)out%_Alignof(XL6_NullableSlotMapKeyV1)==0 &&
    !out->hasValue && !out->value.instance && !out->value.slot;}
static bool begin(xl6_error_scope* scope)
{
    if(xl6ErrorScopeBegin(scope))return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return false;
}
static bool finish(xl6_error_scope* scope,bool ok)
{
    if(!ok || xrtGetError() || scope->failed)xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);
    return result.ok;
}
static bool init(ptr target,const xrttype* type)
{(void)type;*(XL6_NullableSlotMapKeyV1*)target=(XL6_NullableSlotMapKeyV1){0};return true;}
static bool copy(ptr target,const void* source,const xrttype* type)
{
    (void)type;XL6_NullableSlotMapKeyV1* out=target;
    const XL6_NullableSlotMapKeyV1* input=source;
    xl6_error_scope scope={0};if(!begin(&scope))return false;
    XL6_NullableSlotMapKeyV1 acquired={0};bool ok=false;
    if(!valid(*input) || !valid(*out))xrtSetErrorKind(XERR_ARGUMENT);
    else if(target==source)ok=true;
    else if(xrtMemRangesOverlap(target,sizeof(*out),source,sizeof(*input)))xrtSetErrorKind(XERR_ARGUMENT);
    else{
        ok=!input->hasValue || xl6SlotMapKeyCopyValueV1(input->value,&acquired.value);
        if(ok && !xrtGetError() && !scope.failed){
            acquired.hasValue=input->hasValue;
            XL6_NullableSlotMapKeyV1 prior=*out;*out=acquired;acquired=(XL6_NullableSlotMapKeyV1){0};
            xl6NullableSlotMapKeyDropValueV1(prior);
        }else ok=false;
    }
    /* Partial child acquisition is owned even before its presence bit is set. */
    xl6NullableSlotMapKeyDropValueV1(acquired);return finish(&scope,ok);
}
static bool move(ptr target,ptr source,const xrttype* type)
{
    (void)type;XL6_NullableSlotMapKeyV1* out=target,*input=source;
    if(!valid(*input) || !valid(*out) || (target!=source &&
        xrtMemRangesOverlap(target,sizeof(*out),source,sizeof(*input)))){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(target==source)return true;
    XL6_NullableSlotMapKeyV1 prior=*out;*out=*input;*input=(XL6_NullableSlotMapKeyV1){0};
    xl6NullableSlotMapKeyDropValueV1(prior);return true;
}
static void drop(ptr target,const xrttype* type)
{
    (void)type;XL6_NullableSlotMapKeyV1* out=target;
    XL6_NullableSlotMapKeyV1 prior=*out;*out=(XL6_NullableSlotMapKeyV1){0};
    xl6NullableSlotMapKeyDropValueV1(prior);
}
const xrttype* xl6NullableSlotMapKeyTypeV1(void)
{
    static const xrttypeops ops={.Init=init,.Copy=copy,.Move=move,.Drop=drop,.Clone=copy};
    static const xrttype type={.Id=UINT64_C(0x5D2827B4682336ED),.Kind=XRT_TYPE_OPTIONAL,
        .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_NULLABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
        .Name=XRT_STR_INIT("nullable<SlotMapKey>"),.AbiName=XRT_STR_INIT("xlang.runtime.nullable-slot-map-key.v1"),
        .Size=sizeof(XL6_NullableSlotMapKeyV1),.Align=_Alignof(XL6_NullableSlotMapKeyV1),.InstanceAlign=1,.Ops=&ops};
    return &type;
}
void xl6NullableSlotMapKeyDropValueV1(XL6_NullableSlotMapKeyV1 value)
{xl6SlotMapKeyDropValueV1(value.value);}
bool xl6NullableSlotMapKeyEqualV1(XL6_NullableSlotMapKeyV1 left,XL6_NullableSlotMapKeyV1 right)
{return valid(left) && valid(right) && left.hasValue==right.hasValue &&
    (!left.hasValue || xl6SlotMapKeyEqualV1(left.value,right.value));}
bool xl6NullableSlotMapKeyCopyValueV1(XL6_NullableSlotMapKeyV1 input,XL6_NullableSlotMapKeyV1* out)
{
    xl6_error_scope scope={0};if(!begin(&scope))return false;bool ok=false;
    if(!empty(out) || !valid(input))xrtSetErrorKind(XERR_ARGUMENT);
    else ok=copy(out,&input,xl6NullableSlotMapKeyTypeV1());
    return finish(&scope,ok);
}
bool xl6NullableSlotMapKeyWrapV1(XL6_SlotMapKeyV1 input,XL6_NullableSlotMapKeyV1* out)
{return xl6NullableSlotMapKeyCopyValueV1((XL6_NullableSlotMapKeyV1){true,input},out);}
bool xl6NullableSlotMapKeyProjectV1(XL6_NullableSlotMapKeyV1 input,XL6_SlotMapKeyV1* out)
{
    xl6_error_scope scope={0};if(!begin(&scope))return false;bool ok=false;
    XL6_SlotMapKeyV1 acquired={0};
    if(!valid(input) || !out || (uintptr_t)out%_Alignof(XL6_SlotMapKeyV1) || out->instance || out->slot)
        xrtSetErrorKind(XERR_ARGUMENT);
    else if(!input.hasValue)xrtSetErrorKind(XERR_STATE);
    else ok=xl6SlotMapKeyCopyValueV1(input.value,&acquired);
    if(ok && !xrtGetError() && !scope.failed){*out=acquired;acquired=(XL6_SlotMapKeyV1){0};}
    else ok=false;
    xl6SlotMapKeyDropValueV1(acquired);
    return finish(&scope,ok);
}
xvalue* xl6NullableSlotMapKeyBoxV1(XL6_NullableSlotMapKeyV1 input)
{
    xl6_error_scope scope={0};if(!begin(&scope))return NULL;xvalue* out=NULL;
    if(!valid(input))xrtSetErrorKind(XERR_ARGUMENT);
    else out=input.hasValue?xl6SlotMapKeyBoxV1(input.value):xrtValueNull();
    if(!finish(&scope,out!=NULL)){xrtValueRelease(out);return NULL;}return out;
}
bool xl6NullableSlotMapKeyUnboxV1(const xvalue* input,XL6_NullableSlotMapKeyV1* out)
{
    xl6_error_scope scope={0};if(!begin(&scope))return false;bool ok=false;
    XL6_NullableSlotMapKeyV1 acquired={0};
    if(!input || !empty(out))xrtSetErrorKind(XERR_ARGUMENT);
    else if(xrtValueType(input)==XVALUE_NULL)ok=true;
    else if(xl6SlotMapKeyUnboxV1(input,&acquired.value)){acquired.hasValue=true;ok=true;}
    if(ok && !xrtGetError() && !scope.failed){*out=acquired;acquired=(XL6_NullableSlotMapKeyV1){0};}
    else ok=false;
    xl6NullableSlotMapKeyDropValueV1(acquired);return finish(&scope,ok);
}
