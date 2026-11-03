#include "src/common/xl6_xrt.h"
#include "xl6_foreign_scope_value.h"
#include "xl6_sequence.h"
#include "xl6_array_extract_view.h"
#include "xl6_ownership_domain_impl.h"
#include <stdlib.h>

static bool valid(XL6_ForeignScopeViewV1 v){return !!v.address==!!v.lifetime;}
static bool absent(XL6_ForeignScopeViewV1 v){return !v.address && !v.lifetime;}
static bool optionalValid(XL6_NullableForeignScopeViewV1 v)
{return v.hasValue ? valid(v.value) && !absent(v.value) : absent(v.value);}
static bool equal(XL6_ForeignScopeViewV1 a,XL6_ForeignScopeViewV1 b)
{return a.address==b.address && a.lifetime==b.lifetime;}
bool xl6ScopeViewCopyV1(XL6_ForeignScopeViewV1 v,XL6_ForeignScopeViewV1* out)
{
    if(!out || (uintptr_t)out%_Alignof(XL6_ForeignScopeViewV1) || !absent(*out) || !valid(v)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    *out=v;return true;
}
void xl6ScopeViewDropV1(XL6_ForeignScopeViewV1 v){(void)v;}
bool xl6ScopeViewEqualV1(XL6_ForeignScopeViewV1 a,XL6_ForeignScopeViewV1 b){return equal(a,b);}
bool xl6NullableScopeViewCopyV1(XL6_NullableForeignScopeViewV1 v,XL6_NullableForeignScopeViewV1* out)
{
    if(!out || (uintptr_t)out%_Alignof(XL6_ForeignScopeViewV1) || out->hasValue || !absent(out->value) || !optionalValid(v)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    *out=v;return true;
}
void xl6NullableScopeViewDropV1(XL6_NullableForeignScopeViewV1 v){(void)v;}
bool xl6NullableScopeViewEqualV1(XL6_NullableForeignScopeViewV1 a,XL6_NullableForeignScopeViewV1 b)
{return a.hasValue==b.hasValue && (!a.hasValue || equal(a.value,b.value));}

static bool cloneHandle(void* raw,void** out,void* user)
{
    if(!raw || !out || user){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ForeignScopeViewV1* copy=xrtMalloc(sizeof(*copy));if(!copy)return false;
    *copy=*(XL6_ForeignScopeViewV1*)raw;*out=copy;return true;
}
static void dropHandle(void* raw,void* user){if(user)abort();xrtFree(raw);}
static const xvaluehandleops handleOps={cloneHandle,dropHandle,NULL,NULL};
static bool traceHandle(const xvalue* value,xrtownershipvisitor visit,void* user)
{(void)value;(void)visit;(void)user;return true;}
static const xrtownershipadapterv1* adapter(xrtownershipref ref)
{return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,traceHandle);}
static bool getValue(const xvalue* input,uint64_t identity,bool nullable,XL6_ForeignScopeViewV1* out)
{
    if(!input || xrtValueType(input)==XVALUE_NULL){if(nullable)return true;goto typeError;}
    if(xrtValueTypeId(input)!=identity || !adapter(xrtValueOwnership(input)))goto typeError;
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!xrtValueGetHandle(input,&raw,&ops,&user) || ops!=&handleOps || user || !raw)goto typeError;
    XL6_ForeignScopeViewV1 v=*(const XL6_ForeignScopeViewV1*)raw;
    if(!valid(v) || absent(v))goto typeError;
    *out=v;return true;
typeError:xrtSetErrorKind(XERR_TYPE);return false;
}
static xvalue* box(XL6_ForeignScopeViewV1 v,uint64_t identity)
{
    if(!valid(v)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(absent(v))return xrtValueNull();
    XL6_ForeignScopeViewV1* copy=xrtMalloc(sizeof(*copy));if(!copy)return NULL;
    *copy=v;void* raw=copy;xvalue* result=xrtValueHandleTake(&raw,&handleOps,NULL);
    if(raw)xrtFree(raw);
    if(result && (!xrtValueHandleOwnershipBindPhased(result,traceHandle) ||
        (identity && !xrtValueTypeIdBind(result,identity)))){xrtValueRelease(result);return NULL;}
    return result;
}
xvalue* xl6ScopeViewBoxV1(XL6_ForeignScopeViewV1 v,uint64_t id)
{if(!id){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return box(v,id);}
xvalue* xl6NullableScopeViewBoxV1(XL6_NullableForeignScopeViewV1 v,uint64_t id)
{if(!id || !optionalValid(v)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return box(v.value,id);}
bool xl6ScopeViewUnboxV1(const xvalue* v,uint64_t id,bool nullable,XL6_ForeignScopeViewV1* out)
{
    if(!id || nullable || !out || (uintptr_t)out%_Alignof(XL6_ForeignScopeViewV1) || !absent(*out)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return getValue(v,id,false,out);
}
bool xl6NullableScopeViewUnboxV1(const xvalue* v,uint64_t id,bool nullable,XL6_NullableForeignScopeViewV1* out)
{
    if(!id || !nullable || !out || (uintptr_t)out%_Alignof(XL6_ForeignScopeViewV1) || out->hasValue || !absent(out->value)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    XL6_ForeignScopeViewV1 item={0};if(!getValue(v,id,true,&item))return false;
    *out=(XL6_NullableForeignScopeViewV1){!absent(item),item};return true;
}
static bool admits(const xvalue* v,uint64_t id,bool nullable)
{return !v || xrtValueType(v)==XVALUE_NULL ? nullable : xrtValueTypeId(v)==id && adapter(xrtValueOwnership(v));}
bool xl6ScopeViewTryUnboxV1(const xvalue* v,uint64_t id,XL6_ForeignScopeViewV1* out)
{return xl6ScopeViewUnboxV1(v,id,false,out);}
bool xl6NullableScopeViewTryUnboxV1(const xvalue* v,uint64_t id,XL6_NullableForeignScopeViewV1* out)
{
    if(!id || !out || (uintptr_t)out%_Alignof(XL6_ForeignScopeViewV1) || out->hasValue || !absent(out->value)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return !admits(v,id,true) || xl6NullableScopeViewUnboxV1(v,id,true,out);
}

/* Exact resident carrier profiles. No native addresses are followed by copy,
 * trace, capture, container access or destruction. */
#define SCOPE_STORAGE(Tag,Type,Identity) \
    static bool Tag##Init(ptr p,const xrttype* t){(void)t;*(Type*)p=(Type){0};return true;} \
    static bool Tag##Copy(ptr p,const void* s,const xrttype* t){(void)t;*(Type*)p=*(const Type*)s;return true;} \
    static bool Tag##Move(ptr p,ptr s,const xrttype* t){(void)t;if(p!=s){*(Type*)p=*(Type*)s;*(Type*)s=(Type){0};}return true;} \
    static void Tag##Drop(ptr p,const xrttype* t){(void)t;*(Type*)p=(Type){0};} \
    const xrttype* xl6##Tag##TypeV1(void){ \
        static const xrttypeops ops={.Init=Tag##Init,.Copy=Tag##Copy,.Move=Tag##Move,.Drop=Tag##Drop,.Clone=Tag##Copy}; \
        static const xrttype type={.Id=Identity,.Kind=XRT_TYPE_RECORD, \
            .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE, \
            .Name=XRT_STR_INIT(#Tag),.AbiName=XRT_STR_INIT("xl6." #Tag ".v1"),.Size=sizeof(Type),.Align=_Alignof(Type),.InstanceAlign=1,.Ops=&ops};return &type;} \
    static void Tag##Detach(void* raw,unsigned char* live){*(Type*)raw=(Type){0};if(live)*live=0;} \
    const XL6_NativeCaptureProfileV1* xl6##Tag##CaptureV1(void){ \
        static const XL6_NativeCaptureProfileV1 profile={sizeof(profile),Identity,xl6##Tag##TypeV1,Tag##Detach};return &profile;}
SCOPE_STORAGE(ScopeView,XL6_ForeignScopeViewV1,UINT64_C(0xB42542E0F469CF6D))
SCOPE_STORAGE(NullableScopeView,XL6_NullableForeignScopeViewV1,UINT64_C(0x550BC4E0799BBC2C))
#undef SCOPE_STORAGE
static bool toTyped(const xvalue* v,const xrttype* type,ptr out,ptr user)
{
    if(user){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ForeignScopeViewV1 item={0};
    /* Admission checked the nominal identity before this private conversion. */
    uint64_t id=v ? xrtValueTypeId(v) : 0;
    if(type==xl6ScopeViewTypeV1())return getValue(v,id,false,out);
    if(type!=xl6NullableScopeViewTypeV1()){xrtSetErrorKind(XERR_TYPE);return false;}
    if(!getValue(v,id,true,&item))return false;
    *(XL6_NullableForeignScopeViewV1*)out=(XL6_NullableForeignScopeViewV1){!absent(item),item};return true;
}
static xvalue* fromTyped(const xrttype* type,const void* in,ptr user)
{
    if(user || !in){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(type==xl6ScopeViewTypeV1())return box(*(const XL6_ForeignScopeViewV1*)in,0);
    if(type==xl6NullableScopeViewTypeV1())return box(((const XL6_NullableForeignScopeViewV1*)in)->value,0);
    xrtSetErrorKind(XERR_TYPE);return NULL;
}
const xvalueconverter* xl6ScopeViewConverterV1(void)
{static const xvalueconverter converter={.ToTyped=toTyped,.FromTyped=fromTyped};return &converter;}
const xvalueconverter* xl6NullableScopeViewConverterV1(void){return xl6ScopeViewConverterV1();}
static bool resolveDomain(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** out,const xrtownershippreparationv1** prep,const XL6_OwnershipFinishV1** finish)
{
    (void)policies;(void)prep;(void)finish;const xrtownershipadapterv1* a=adapter(ref);
    if(!a)return false;
    *out=a;return true;
}
const XL6_OwnershipDomainV1* xl6OwnershipDomainScopeViewValueV1Get(void)
{static const XL6_OwnershipDomainV1 domain={sizeof(domain),NULL,resolveDomain,NULL};return &domain;}

typedef struct ScopePolicy {
    XL6_SequenceNativeValuePolicyV1 contract;
    volatile int32_t refs;
    uint64_t element,boxed;
    bool nullable;
} ScopePolicy;
static bool slot(const void* raw,const void* item)
{
    if(!item)return false;
    return ((const ScopePolicy*)raw)->nullable ? optionalValid(*(const XL6_NullableForeignScopeViewV1*)item) :
        valid(*(const XL6_ForeignScopeViewV1*)item) && !absent(*(const XL6_ForeignScopeViewV1*)item);
}
static bool retainPolicy(const void* raw)
{if(raw && xrtRefRetain(&((ScopePolicy*)raw)->refs)>0)return true;xrtSetErrorKind(XERR_STATE);return false;}
static void dropPolicy(const void* raw)
{ScopePolicy* p=(ScopePolicy*)raw;int32_t refs=xrtRefRelease(&p->refs);if(refs<0)abort();if(!refs)xrtFree(p);}
static bool value(const void* raw,const xvalue* item)
{const ScopePolicy* p=raw;return admits(item,p->boxed,p->nullable);}
static bool equivalent(const void* a,const void* b)
{const ScopePolicy* x=a;const ScopePolicy* y=b;return x->element==y->element && x->boxed==y->boxed && x->nullable==y->nullable;}
static bool defaultable(const void* raw){return ((const ScopePolicy*)raw)->nullable;}
static bool traceSlot(const void* raw,const void* item,xrtownershipvisitor visit,void* context)
{(void)context;return visit && slot(raw,item);}
static const XL6_SequenceAdmissionOpsV2 admission={retainPolicy,dropPolicy,slot,value,equivalent,defaultable,NULL,traceSlot};
const XL6_SequenceAdmissionOpsV2* xl6ScopeViewSequenceAdmissionV1(void){return &admission;}
static bool empty(const void* raw){return raw && absent(*(const XL6_ForeignScopeViewV1*)raw);}
static bool nullableEmpty(const void* raw)
{const XL6_NullableForeignScopeViewV1* v=raw;return v && !v->hasValue && absent(v->value);}
static bool copyFresh(const void* in,void* out)
{return in && xl6ScopeViewCopyV1(*(const XL6_ForeignScopeViewV1*)in,out);}
static bool nullableCopyFresh(const void* in,void* out)
{return in && xl6NullableScopeViewCopyV1(*(const XL6_NullableForeignScopeViewV1*)in,out);}
static uint64 hashValue(XL6_ForeignScopeViewV1 v)
{return (uint64)v.address ^ (v.lifetime*UINT64_C(0x9E3779B97F4A7C15));}
static uint64 hashSlot(const void* slot,ptr user){(void)user;return hashValue(*(const XL6_ForeignScopeViewV1*)slot);}
static bool equalSlot(const void* a,const void* b,ptr user)
{(void)user;return equal(*(const XL6_ForeignScopeViewV1*)a,*(const XL6_ForeignScopeViewV1*)b);}
static uint64 hashNullable(const void* slot,ptr user)
{(void)user;const XL6_NullableForeignScopeViewV1* v=slot;return v->hasValue ? hashValue(v->value) : 0;}
static bool equalNullable(const void* a,const void* b,ptr user)
{(void)user;return xl6NullableScopeViewEqualV1(*(const XL6_NullableForeignScopeViewV1*)a,*(const XL6_NullableForeignScopeViewV1*)b);}
static const xtypedsetkeypolicy content={hashSlot,equalSlot,NULL},nullableContent={hashNullable,equalNullable,NULL};
xvalue* xl6ScopeViewSequenceCreateV1(unsigned int role,uint64_t owner,uint64_t element,uint64_t boxed,bool nullable)
{
    if(!owner || !element || !boxed || (role!=XL6_SEQUENCE_ARRAY_V1 && role!=XL6_SEQUENCE_STACK_V1)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    ScopePolicy* p=xrtCalloc(1,sizeof(*p));if(!p)return NULL;
    const xrttype* type=nullable ? xl6NullableScopeViewTypeV1() : xl6ScopeViewTypeV1();
    p->contract=(XL6_SequenceNativeValuePolicyV1){type,nullable ? &nullableContent : &content,
        nullable ? nullableEmpty : empty,nullable ? nullableCopyFresh : copyFresh};
    p->refs=1;p->element=element;p->boxed=boxed;p->nullable=nullable;
    XL6_SequenceAdmissionV3 policy={&admission,p,element,XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,0,type->Size,type,boxed};
    xvalue* result=xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,owner,&policy);
    /* CreateAdmitted consumes the private policy on both outcomes. */
    if(result){
        XL6_Sequence* pin=xl6SequenceBeginRoleV1(result,(XL6_SequenceRoleV1)role,owner,element,
            XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,type->Size,type);
        bool ok=pin && xl6SequenceBindProjectionRoleV1(pin,(XL6_SequenceRoleV1)role,xl6ScopeViewConverterV1());
        if(pin)xl6SequenceEndV1(pin);
        if(!ok){xrtValueRelease(result);return NULL;}
    }
    return result;
}
XL6_OWNERSHIP_DOMAIN_POLICY(SequenceScopeView,Sequence,xl6ScopeViewSequenceAdmissionV1)
bool xl6ArrayExtractScopeViewV1(const xvalue* source,uint64_t owner,uint64_t element,
    int64_t index,bool pop,uint64_t boxed,bool nullable,void* output)
{
    XL6_ArrayExtractView view={0};bool ok=false;
    const xrttype* type=nullable ? xl6NullableScopeViewTypeV1() : xl6ScopeViewTypeV1();
    if(!output || (uintptr_t)output%type->Align || !boxed ||
        !(nullable ? nullableEmpty(output) : empty(output))){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(xl6ArrayExtractBegin(source,owner,element,index,pop,&view)){
        if(view.owner){
            XL6_SequenceAdmissionV3 policy={0};
            if(xl6ArrayExtractSlotMatches(&view,XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,type->Size,type) &&
                xl6SequenceAdmissionCopyV3(view.owner,&policy)){
                const ScopePolicy* p=policy.ops==&admission ? policy.data : NULL;
                xtypedarray* storage=xl6SequenceStorageV1(view.owner);
                if(!p || p->element!=element || p->boxed!=boxed || p->nullable!=nullable)xrtSetErrorKind(XERR_TYPE);
                else if(xrtTypedArrayCapacity(storage)>SIZE_MAX/type->Size)xrtSetErrorKind(XERR_STATE);
                else if(xrtMemRangesOverlap(xrtTypedArrayConstGet(storage,0),xrtTypedArrayCapacity(storage)*type->Size,output,type->Size))
                    xrtSetErrorKind(XERR_ARGUMENT);
                else ok=xrtTypedArrayTake(storage,view.at,output);
            }
            xl6SequenceAdmissionDropV3(&policy);
        }else {
            ok=nullable ? xl6NullableScopeViewUnboxV1(view.item,boxed,true,output) : xl6ScopeViewUnboxV1(view.item,boxed,false,output);
            if(ok)ok=xl6ArrayExtractWireCommit(&view);
        }
    }
    return xl6ArrayExtractFinish(&view,ok);
}
