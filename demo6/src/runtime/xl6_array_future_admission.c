#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include <stdlib.h>

typedef struct FuturePolicy {
    volatile int32_t refs;
    uint64_t identity;
    bool nullable;
} FuturePolicy;
static bool retain(const void* opaque)
{
    FuturePolicy* policy=(FuturePolicy*)opaque;
    if (!policy || xrtRefRetain(&policy->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void drop(const void* opaque)
{
    FuturePolicy* policy=(FuturePolicy*)opaque;
    int32_t refs=xrtRefRelease(&policy->refs); if (refs<0) abort();
    if (!refs) xrtFree(policy);
}
static bool slot(const void* opaque,const void* item)
{
    const FuturePolicy* policy=opaque;
    return item && (policy->nullable || *(xfuture* const*)item!=NULL);
}
static bool value(const void* opaque,const xvalue* item)
{
    const FuturePolicy* policy=opaque;
    if (!item || xrtValueType(item)==XVALUE_NULL) return policy->nullable;
    return xrtValueTypeId(item)==policy->identity && xrtValueIsFuture(item);
}
static bool equivalent(const void* left,const void* right)
{
    const FuturePolicy* a=left; const FuturePolicy* b=right;
    return a->identity==b->identity && a->nullable==b->nullable;
}
static bool defaultable(const void* opaque) { return ((const FuturePolicy*)opaque)->nullable; }
static bool trace(const void* opaque,const void* item,xrtownershipvisitor visit,void* context)
{
    (void)opaque;
    xfuture* future=*(xfuture* const*)item;
    return !future || visit(xrtFutureOwnership(future),context);
}
static const XL6_SequenceAdmissionOpsV2 ops={retain,drop,slot,value,equivalent,defaultable,NULL,trace};
const XL6_SequenceAdmissionOpsV2* xl6ArrayFutureAdmissionOpsV2Get(void) { return &ops; }
xvalue* xl6ArrayFutureCreateV1(uint64_t identity,uint64_t element,uint64_t baseIdentity,bool nullable)
{
    if (!identity || !element || !baseIdentity ||
        (nullable ? element==baseIdentity : element!=baseIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    FuturePolicy* policy=xrtMalloc(sizeof(*policy));
    if (!policy) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *policy=(FuturePolicy){.refs=1,.identity=baseIdentity,.nullable=nullable};
    const xrttype* type=xrtTypeFuture();
    XL6_SequenceAdmissionV3 admission={&ops,policy,element,XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1,
        sizeof(xfuture*),type,baseIdentity};
    xvalue* result=xl6ArrayCreateV1(identity,element,admission.transfer,admission.size,type);
    if (result) {
        XL6_Sequence* owner=xl6SequenceBeginRoleV1(result,XL6_SEQUENCE_ARRAY_V1,
            identity,element,admission.transfer,admission.size,type);
        if (!owner || !xl6SequenceAdmissionTakeV3(owner,&admission)) {
            if (owner) xl6SequenceEndV1(owner);
            xerror* error=xrtTakeError(); xrtValueRelease(result); result=NULL;
            xrtClearError(); xrtSetErrorTake(error);
        } else xl6SequenceEndV1(owner);
    }
    xl6SequenceAdmissionDropV3(&admission); return result;
}
