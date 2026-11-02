#include "xl6_foreign_view_sequence.h"
#include "xl6_sequence.h"
#include "xl6_foreign_view_value_storage.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef struct ViewPolicy {
    XL6_SequenceNativeValuePolicyV1 contract;
    volatile int32_t refs;
    uint64_t element,boxed;
    bool nullable;
} ViewPolicy;
static void mutationBegin(xrtownershipscope* scope)
{if(!xrtOwnershipMutationBegin(scope))abort();}
static void mutationEnd(xrtownershipscope* scope)
{if(!xrtOwnershipScopeEnd(scope))abort();}
static void release(XL6_ForeignView* owned)
{
    if(!owned)return;
    /* A typed-array operation may still own private slot addresses. Native
     * destruction is queued until the caller reaches a safe drain boundary. */
    XL6_ForeignCallbackScopeV1 callback={0};
    if(!xl6ForeignCallbackEnterV1(&callback))abort();
    if(!xl6ForeignViewReleaseV1(owned))xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    if(!xl6ForeignCallbackLeaveV1(&callback))abort();
}
static bool init(ptr target,const xrttype* type)
{(void)type;*(XL6_ForeignView**)target=NULL;return true;}
static bool copy(ptr target,const void* source,const xrttype* type)
{
    (void)type;if(target==source)return true;
    XL6_ForeignView* input=*(XL6_ForeignView* const*)source;
    XL6_ForeignView* acquired=xl6ForeignViewRetainV1(input);
    if(input && !acquired)return false;
    xrtownershipscope mutation={0};mutationBegin(&mutation);
    XL6_ForeignView* prior=*(XL6_ForeignView**)target;
    *(XL6_ForeignView**)target=acquired;mutationEnd(&mutation);
    release(prior);return true;
}
static bool move(ptr target,ptr source,const xrttype* type)
{
    (void)type;if(target==source)return true;
    xrtownershipscope mutation={0};mutationBegin(&mutation);
    XL6_ForeignView* prior=*(XL6_ForeignView**)target;
    *(XL6_ForeignView**)target=*(XL6_ForeignView**)source;
    *(XL6_ForeignView**)source=NULL;mutationEnd(&mutation);
    release(prior);return true;
}
static void dropSlot(ptr slot,const xrttype* type)
{
    (void)type;xrtownershipscope mutation={0};mutationBegin(&mutation);
    XL6_ForeignView* owned=*(XL6_ForeignView**)slot;
    *(XL6_ForeignView**)slot=NULL;mutationEnd(&mutation);release(owned);
}
const xrttype* xl6ForeignViewStorageTypeV1(void)
{
    static const xrttypeops ops={.Init=init,.Copy=copy,.Move=move,.Drop=dropSlot,.Clone=copy};
    static const xrttype type={.Id=UINT64_C(0x881DFDD63DC6C9CD),.Kind=XRT_TYPE_RECORD,
        .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
        .Name=XRT_STR_INIT("foreign-view-slot"),.AbiName=XRT_STR_INIT("xl6.foreign-view-slot.v1"),
        .Size=sizeof(XL6_ForeignView*),.Align=_Alignof(XL6_ForeignView*),.InstanceAlign=1,.Ops=&ops};
    return &type;
}
static bool empty(const void* slot){return slot && !*(XL6_ForeignView* const*)slot;}
static bool copyFresh(const void* input,void* output)
{
    if(!input || !output || (uintptr_t)output%_Alignof(XL6_ForeignView*) || !empty(output)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return copy(output,input,xl6ForeignViewStorageTypeV1());
}
static uint64 hash(const void* slot,ptr context)
{(void)context;return (uint64_t)(uintptr_t)*(XL6_ForeignView* const*)slot;}
static bool equal(const void* a,const void* b,ptr context)
{(void)context;return *(XL6_ForeignView* const*)a==*(XL6_ForeignView* const*)b;}
static const xtypedsetkeypolicy content={hash,equal,NULL};
static bool retainPolicy(const void* raw)
{
    ViewPolicy* p=(ViewPolicy*)raw;
    if(p && xrtRefRetain(&p->refs)>0)return true;
    xrtSetErrorKind(XERR_STATE);return false;
}
static void dropPolicy(const void* raw)
{
    ViewPolicy* p=(ViewPolicy*)raw;int32_t refs=xrtRefRelease(&p->refs);
    if(refs<0)abort();
    if(!refs)xrtFree(p);
}
static bool slot(const void* raw,const void* item)
{return item && (((const ViewPolicy*)raw)->nullable || *(XL6_ForeignView* const*)item);}
static bool value(const void* raw,const xvalue* item)
{const ViewPolicy* p=raw;return xl6ForeignViewValueAdmitsV1(item,p->boxed,p->nullable);}
static bool equivalent(const void* a,const void* b)
{
    const ViewPolicy* left=a;const ViewPolicy* right=b;
    return left->element==right->element && left->boxed==right->boxed && left->nullable==right->nullable;
}
static bool defaultable(const void* raw){return ((const ViewPolicy*)raw)->nullable;}
static bool trace(const void* raw,const void* item,xrtownershipvisitor visit,void* context)
{
    if(!visit || !slot(raw,item))return false;
    XL6_ForeignView* view=*(XL6_ForeignView* const*)item;
    return !view || visit(xl6ForeignViewOwnershipV1(view),context);
}
static const XL6_SequenceAdmissionOpsV2 admission={retainPolicy,dropPolicy,slot,value,equivalent,defaultable,NULL,trace};
const XL6_SequenceAdmissionOpsV2* xl6ForeignViewSequenceAdmissionV1(void){return &admission;}
static bool toTyped(const xvalue* input,const xrttype* type,ptr output,ptr context)
{
    if(context || type!=xl6ForeignViewStorageTypeV1()){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return xl6ForeignViewStorageUnboxV1(input,output);
}
static xvalue* fromTyped(const xrttype* type,const void* input,ptr context)
{
    if(context || !input || type!=xl6ForeignViewStorageTypeV1()){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    return xl6ForeignViewStorageBoxV1(*(XL6_ForeignView* const*)input);
}
const xvalueconverter* xl6ForeignViewSequenceConverterV1(void)
{static const xvalueconverter converter={.ToTyped=toTyped,.FromTyped=fromTyped};return &converter;}
xvalue* xl6ForeignViewSequenceCreateV1(unsigned int role,uint64_t owner,
    uint64_t element,uint64_t boxed,bool nullable)
{
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return NULL;
    xvalue* result=NULL;ViewPolicy* p=NULL;
    if(!owner || !element || !boxed || (role!=XL6_SEQUENCE_ARRAY_V1 && role!=XL6_SEQUENCE_STACK_V1))
        xrtSetErrorKind(XERR_ARGUMENT);
    else p=xrtCalloc(1,sizeof(*p));
    if(p){
        const xrttype* type=xl6ForeignViewStorageTypeV1();
        p->contract=(XL6_SequenceNativeValuePolicyV1){type,&content,empty,copyFresh};
        p->refs=1;p->element=element;p->boxed=boxed;p->nullable=nullable;
        XL6_SequenceAdmissionV3 policy={&admission,p,element,XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,0,type->Size,type,boxed};
        result=xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,owner,&policy);
        if(result){
            XL6_Sequence* pin=xl6SequenceBeginRoleV1(result,(XL6_SequenceRoleV1)role,owner,element,
                XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,type->Size,type);
            bool ok=pin && xl6SequenceBindProjectionRoleV1(pin,(XL6_SequenceRoleV1)role,xl6ForeignViewSequenceConverterV1());
            if(pin)xl6SequenceEndV1(pin);
            if(!ok)xl6ErrorScopeCaptureFailure(&errors);
        }
    }
    if(!result || xrtGetError() || errors.failed){
        xl6ErrorScopeCaptureFailure(&errors);xrtValueRelease(result);result=NULL;
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){xrtSetErrorTake(outcome.error);return NULL;}
    return result;
}
XL6_OWNERSHIP_DOMAIN_POLICY(SequenceForeignView,Sequence,xl6ForeignViewSequenceAdmissionV1)
