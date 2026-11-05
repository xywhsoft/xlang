#define XRT_MODULE_MEMORY
#define XRT_MODULE_VALUE
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_THREAD
#include "src/common/xl6_xrt.h"
#include "xl6_native_sequence.h"
#include "xl6_callback_failure.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

typedef struct NativeBacking {
    const XL6_ForeignViewDescriptorV1* policy;
    const void* native;
    const void* data;
    size_t count;
    uint64_t sequence;
} NativeBacking;
typedef struct NativeRange {
    XL6_NativeSequenceSchemaV1 schema;
    XL6_ForeignView* owner;
    size_t offset,count;
} NativeRange;

static bool schemaValid(const XL6_NativeSequenceSchemaV1* s)
{
    return s && s->size==sizeof(*s) && s->abiTag==XL6_NATIVE_SEQUENCE_SCHEMA_V1_TAG &&
        s->sequenceIdentity && s->elementIdentity && s->elementSize && s->elementAlignment &&
        !(s->elementAlignment&(s->elementAlignment-1)) && !(s->elementSize%s->elementAlignment) &&
        s->stride>=s->elementSize && !(s->stride%s->elementAlignment);
}
static bool schemaEqual(const XL6_NativeSequenceSchemaV1* a,const XL6_NativeSequenceSchemaV1* b)
{
    return a->sequenceIdentity==b->sequenceIdentity && a->elementIdentity==b->elementIdentity &&
        a->elementSize==b->elementSize && a->elementAlignment==b->elementAlignment && a->stride==b->stride;
}
static bool publish(xl6_error_result result)
{
    if(!result.ok)xl6ErrorScopePropagateFailure(result);else xrtErrorFree(result.error);
    return result.ok;
}
static bool backingDestroy(void* raw)
{
    NativeBacking* b=raw;
    if(!b->native){xrtFree(b);return true;}
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return false;
    bool consumed=b->policy->Destroy((void*)b->native);
    bool independent=(b->policy->flags&(XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE))!=0;
    if((!consumed&&!independent)||xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    if(consumed)xrtFree(b);
    (void)publish(xl6ErrorScopeEnd(&errors));return consumed;
}
static bool backingTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{
    const NativeBacking* b=raw;
    return !b->native || !b->policy->TraceRaw || b->policy->TraceRaw(b->native,visit,user);
}
static bool backingBusy(const void* raw)
{
    const NativeBacking* b=raw;
    return b->native && b->policy->Busy && b->policy->Busy(b->native);
}
static const XL6_ForeignViewDescriptorV1 backingPolicy={sizeof(backingPolicy),XL6_FOREIGN_VIEW_V1_TAG,
    "xl6.native-sequence.backing.v1",0,1,NULL,NULL,backingDestroy,0,1,NULL,NULL,backingTrace,backingBusy,
    XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,backingDestroy,NULL};

static void releaseFailure(xl6_error_result failure)
{
    if(xl6ErrorScopeTryPropagateFailureV1(&failure))return;
    xerror* diagnostic=xrtErrorRef(failure.error);
    xl6CallbackFailureRecordV1(failure);xrtSetErrorTake(diagnostic);
}
static void rangeDrop(void* raw,void* user)
{
    if(user)abort();
    NativeRange* r=raw;if(!r)return;
    XL6_ForeignView* owner=r->owner;r->owner=NULL;xrtFree(r);
    if(!owner)return;
    XL6_ForeignCallbackScopeV1 callback={0};if(!xl6ForeignCallbackEnterV1(&callback))abort();
    if(!xl6ForeignViewReleaseV1(owner))releaseFailure((xl6_error_result){false,xrtTakeError()});
    if(!xl6ForeignCallbackLeaveV1(&callback))releaseFailure((xl6_error_result){false,xrtTakeError()});
}
static bool rangeClone(void* raw,void** output,void* user)
{
    if(!raw || !output || user){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    const NativeRange* old=raw;NativeRange* copy=xrtMalloc(sizeof(*copy));if(!copy)return false;
    *copy=*old;copy->owner=old->owner?xl6ForeignViewRetainV1(old->owner):NULL;
    if(old->owner&&!copy->owner){xrtFree(copy);return false;}*output=copy;return true;
}
static const xvaluehandleops rangeOps={rangeClone,rangeDrop,NULL,NULL};
static NativeRange* get(const xvalue* value)
{
    if(!value || xrtValueType(value)!=XVALUE_HANDLE)return NULL;
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!xrtValueGetHandle(value,&raw,&ops,&user)||ops!=&rangeOps||user||!raw)return NULL;
    return raw;
}
static bool rangeTrace(const xvalue* value,xrtownershipvisitor visit,void* user)
{
    const NativeRange* r=get(value);
    return r && visit && (!r->owner || visit(xl6ForeignViewOwnershipV1(r->owner),user));
}
static xvalue* wrap(NativeRange* range)
{
    void* slot=range;xvalue* result=xrtValueHandleTake(&slot,&rangeOps,NULL);
    if(!result){if(slot)rangeDrop(slot,NULL);return NULL;}
    if(!xrtValueHandleOwnershipBindPhased(result,rangeTrace) || !xrtValueTypeIdBind(result,range->schema.sequenceIdentity)){
        xrtValueRelease(result);return NULL;
    }
    return result;
}
static bool prepared(const XL6_NativeSequenceResultV1* result)
{return result && result->self==result && result->thread==xrtThreadCurrentId();}
static bool probeEmpty(const XL6_ForeignResultProbeV1* p)
{
    return !p->self && !p->thread && !p->storage && !p->identity && !p->reservedRoot &&
        !p->root && !p->live && !p->observed && !p->failed;
}
static bool callEmpty(const XL6_ForeignViewCallV1* c)
{
    return !c->self && !c->view && !c->root && !c->raw && !c->nativeStorage &&
        !c->execution.self && !c->execution.older && !c->execution.newer && !c->execution.owner;
}
bool xl6NativeSequenceResultReserveV1(XL6_NativeSequenceResultV1* result,const XL6_NativeSequenceSchemaV1* schema,
    const XL6_ForeignViewDescriptorV1* native,const XL6_OwnershipDomainV1* domain,const XL6_CodeOwner* code)
{
    if(!result || result->self || result->thread || result->backing || result->range || result->value || result->spanReady || !probeEmpty(&result->owner) ||
        !schemaValid(schema) || !native || native->size!=sizeof(*native) || native->abiTag!=XL6_FOREIGN_VIEW_V1_TAG || !native->Destroy ||
        native->Watch || native->Unwatch || native->watchBytes ||
        (native->CallBegin && !(native->flags&XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE))){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return false;
    NativeBacking* backing=xrtCalloc(1,sizeof(*backing));NativeRange* range=NULL;xvalue* value=NULL;
    if(backing){backing->policy=native;backing->sequence=schema->sequenceIdentity;range=xrtCalloc(1,sizeof(*range));}
    if(range){range->schema=*schema;value=wrap(range);range=NULL;}
    if(!value || !backing)xl6ErrorScopeCaptureFailure(&errors);
    if(value && !errors.failed && !xl6ForeignResultProbeReserveRootV1(&result->owner,&backingPolicy,domain,code,0,1,XL6_FOREIGN_RESULT_RECORD_UNADOPTED))
        xl6ErrorScopeCaptureFailure(&errors);
    if(!errors.failed && !xl6ForeignResultProbeOwnRecordV1(&result->owner,backing))xl6ErrorScopeCaptureFailure(&errors);
    if(errors.failed){
        if(result->owner.self){bool owned=result->owner.identity!=NULL;if(!xl6ForeignResultProbeEndV1(&result->owner))xl6ErrorScopeCaptureFailure(&errors);if(owned)backing=NULL;}
        xrtFree(backing);xrtValueRelease(value);return publish(xl6ErrorScopeEnd(&errors));
    }
    result->self=result;result->thread=xrtThreadCurrentId();result->backing=backing;result->range=get(value);result->value=value;
    return publish(xl6ErrorScopeEnd(&errors));
}
bool xl6NativeSequenceResultSpanV1(XL6_NativeSequenceResultV1* result,const void* data,uint64_t count)
{
    if(!prepared(result) || !result->backing || !result->range || result->spanReady){xrtSetErrorKind(XERR_STATE);return false;}
    NativeRange* r=result->range;
    if(count>INT64_MAX || count>SIZE_MAX/r->schema.stride ||
        (count && (!data || (uintptr_t)data%r->schema.elementAlignment ||
         (uintptr_t)data>UINTPTR_MAX-((size_t)(count-1)*r->schema.stride+r->schema.elementSize)))){
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    NativeBacking* b=result->backing;b->data=count?data:NULL;b->count=(size_t)count;r->count=(size_t)count;
    result->spanReady=true;return true;
}
bool xl6NativeSequenceResultAdoptV1(XL6_NativeSequenceResultV1* result,const void* native)
{
    if(!prepared(result)||!result->spanReady||!native||!result->backing||((NativeBacking*)result->backing)->native){xrtSetErrorKind(XERR_STATE);return false;}
    ((NativeBacking*)result->backing)->native=native;
    return xl6ForeignResultProbeStorageAdoptedV1(&result->owner);
}
xvalue* xl6NativeSequenceResultTakeV1(XL6_NativeSequenceResultV1* result)
{
    if(!prepared(result)||!result->value||!result->spanReady||!result->backing||!((NativeBacking*)result->backing)->native){xrtSetErrorKind(XERR_STATE);return NULL;}
    XL6_ForeignView* view=xl6ForeignResultProbeMaterializeRootV1(&result->owner);if(!view)return NULL;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    NativeRange* range=result->range;range->owner=view;xvalue* value=result->value;
    result->value=NULL;result->range=NULL;result->backing=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return value;
}
bool xl6NativeSequenceResultEndV1(XL6_NativeSequenceResultV1* result)
{
    if(!result){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!result->self && !result->thread && !result->backing && !result->range && !result->value && !result->spanReady && probeEmpty(&result->owner))return true;
    if(!prepared(result)){xrtSetErrorKind(XERR_STATE);return false;}
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return false;
    xvalue* value=result->value;result->value=NULL;result->range=NULL;result->backing=NULL;
    xrtValueRelease(value);if(xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    if(!xl6ForeignResultProbeEndV1(&result->owner)||xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    *result=(XL6_NativeSequenceResultV1){0};return publish(xl6ErrorScopeEnd(&errors));
}
xvalue* xl6NativeSequenceEmptyV1(const XL6_NativeSequenceSchemaV1* schema)
{
    if(!schemaValid(schema)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    NativeRange* r=xrtCalloc(1,sizeof(*r));if(!r)return NULL;r->schema=*schema;return wrap(r);
}
bool xl6NativeSequenceAdmitsV1(const xvalue* value,uint64_t sequence,uint64_t element)
{
    const NativeRange* r=get(value);
    return sequence && element && r && xrtValueTypeId(value)==sequence &&
        r->schema.sequenceIdentity==sequence && r->schema.elementIdentity==element;
}
bool xl6NativeSequenceLengthV1(const xvalue* value,uint64_t sequence,uint64_t element,int64_t* output)
{
    if(!output || !xl6NativeSequenceAdmitsV1(value,sequence,element)){xrtSetErrorKind(XERR_TYPE);return false;}
    *output=(int64_t)get(value)->count;return true;
}
xvalue* xl6NativeSequenceSliceV1(const xvalue* value,uint64_t sequence,uint64_t element,int64_t start,int64_t count)
{
    if(!xl6NativeSequenceAdmitsV1(value,sequence,element)){xrtSetErrorKind(XERR_TYPE);return NULL;}
    const NativeRange* old=get(value);
    if(start<0 || (uint64_t)start>old->count || count< -1 || (count>=0 && (uint64_t)count>old->count-(size_t)start)){
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    NativeRange* r=xrtMalloc(sizeof(*r));if(!r)return NULL;*r=*old;r->owner=NULL;
    r->offset+=(size_t)start;r->count=count<0?old->count-(size_t)start:(size_t)count;
    if(old->owner && !(r->owner=xl6ForeignViewRetainV1(old->owner))){xrtFree(r);return NULL;}
    return wrap(r);
}
bool xl6NativeSequenceReadBeginV1(const xvalue* value,const XL6_NativeSequenceSchemaV1* schema,XL6_NativeSequenceReadV1* span)
{
    if(!span || span->self || span->data || span->count || span->stride || span->thread || !callEmpty(&span->call) ||
        !schemaValid(schema) || !xl6NativeSequenceAdmitsV1(value,schema->sequenceIdentity,schema->elementIdentity)){
        xrtSetErrorKind(XERR_TYPE);return false;
    }
    const NativeRange* r=get(value);if(!schemaEqual(schema,&r->schema)){xrtSetErrorKind(XERR_TYPE);return false;}
    if(r->owner){
        if(!xl6ForeignViewCallBeginV1(r->owner,&span->call,NULL,0))return false;
        const NativeBacking* b=span->call.raw;
        if(!b || !b->native || b->sequence!=schema->sequenceIdentity || r->offset>b->count || r->count>b->count-r->offset){
            xrtSetErrorKind(XERR_STATE);xerror* first=xrtTakeError();(void)xl6ForeignViewCallEndV1(&span->call);xrtSetErrorTake(first);return false;
        }
        span->data=r->count?(const uint8_t*)b->data+r->offset*schema->stride:NULL;
    }else if(r->offset || r->count){xrtSetErrorKind(XERR_STATE);return false;}
    span->self=span;span->count=r->count;span->stride=schema->stride;span->thread=xrtThreadCurrentId();return true;
}
bool xl6NativeSequenceReadEndV1(XL6_NativeSequenceReadV1* span)
{
    if(!span){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!span->self && !span->thread && !span->data && !span->count && !span->stride && callEmpty(&span->call))return true;
    if(span->self!=span || span->thread!=xrtThreadCurrentId()){xrtSetErrorKind(XERR_STATE);return false;}
    bool ok=xl6ForeignViewCallEndV1(&span->call);
    if(callEmpty(&span->call))*span=(XL6_NativeSequenceReadV1){0};
    return ok;
}
static bool resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{
    (void)policies;(void)preparation;(void)completion;
    const xrtownershipadapterv1* a=xrtValueHandleOwnershipAdapterV1(ref,&rangeOps,rangeTrace);
    if(!a)return false;
    *adapter=a;return true;
}
static bool admitsRead(xrtownershipref ref)
{return xrtValueHandleOwnershipAdapterV1(ref,&rangeOps,rangeTrace)!=NULL;}
const XL6_OwnershipDomainV1* xl6OwnershipDomainNativeSequenceV1Get(void)
{static const XL6_OwnershipDomainV1 domain={sizeof(domain),NULL,resolve,admitsRead};return &domain;}
