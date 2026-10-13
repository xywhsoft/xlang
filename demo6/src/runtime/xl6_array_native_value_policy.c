#include "xl6_array_native_value_policy.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>
typedef struct NativePolicy {
    XL6_SequenceNativeValuePolicyV1 contract;
    const XL6_NativeCollectionValueProfileV1* profile;
    volatile int32_t refs;
} NativePolicy;
static bool retain(const void* raw)
{
    NativePolicy* p=(NativePolicy*)raw;
    if(p && xrtRefRetain(&p->refs)>0)return true;
    xrtSetErrorKind(XERR_STATE);return false;
}
static void drop(const void* raw)
{
    NativePolicy* p=(NativePolicy*)raw;int32_t refs=xrtRefRelease(&p->refs);
    if(refs<0)abort();
    if(!refs)xrtFree(p);
}
static bool slot(const void* raw,const void* item)
{return ((const NativePolicy*)raw)->profile->Slot(item);}
static bool value(const void* raw,const xvalue* item)
{return ((const NativePolicy*)raw)->profile->Value(item);}
static bool equivalent(const void* a,const void* b)
{return ((const NativePolicy*)a)->profile==((const NativePolicy*)b)->profile;}
static bool defaultable(const void* raw){(void)raw;return true;}
static bool trace(const void* raw,const void* item,xrtownershipvisitor visit,void* context)
{(void)context;return visit && slot(raw,item);} /* immutable native leaf */
static const XL6_SequenceAdmissionOpsV2 ops={retain,drop,slot,value,equivalent,defaultable,NULL,trace};
const XL6_SequenceAdmissionOpsV2* xl6ArrayNativeValueAdmissionOpsV2Get(void){return &ops;}
xvalue* xl6SequenceNativeValueCreateV1(unsigned int role,uint64_t identity,const XL6_NativeCollectionValueProfileV1* profile)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;
    }
    xvalue* result=NULL;NativePolicy* p=NULL;
    if(!identity || (role!=XL6_SEQUENCE_ARRAY_V1 && role!=XL6_SEQUENCE_STACK_V1) ||
        !xl6NativeCollectionValueProfileValidV1(profile))xrtSetErrorKind(XERR_ARGUMENT);
    else p=xrtCalloc(1,sizeof(*p));
    if(p){
        const xrttype* type=profile->Type();p->refs=1;p->profile=profile;
        p->contract=(XL6_SequenceNativeValuePolicyV1){type,profile->Content(),profile->Empty,profile->Copy};
        XL6_SequenceAdmissionV3 policy={&ops,p,profile->identity,XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,
            type->Size,type,profile->boxedIdentity};
        result=xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,identity,&policy);
    }
    if(!result || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xrtValueRelease(result);xl6ErrorScopePropagateFailure(outcome);return NULL;}
    return result;
}
