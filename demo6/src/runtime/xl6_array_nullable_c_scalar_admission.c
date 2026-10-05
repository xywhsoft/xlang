#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_c_scalar_domain.h"
#include "src/runtime/xl6_scalar_type_abi.h"
#include <stdlib.h>
typedef struct NullableCScalarPolicy {
    volatile int32_t refs;
    const xrttype* base;
    const xrttype* type;
    uint64_t identity;
    size_t offset;
} NullableCScalarPolicy;
static bool retain(const void* opaque)
{
    NullableCScalarPolicy* data=(NullableCScalarPolicy*)opaque;
    if (!data || xrtRefRetain(&data->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void drop(const void* opaque)
{
    NullableCScalarPolicy* data=(NullableCScalarPolicy*)opaque;
    int32_t refs=xrtRefRelease(&data->refs); if (refs<0) abort();
    if (!refs) xrtFree(data);
}
static bool slot(const void* opaque,const void* item)
{
    const NullableCScalarPolicy* data=opaque;
    if (!item) return false;
    unsigned char present=0; memcpy(&present,item,1);
    if (present>1) return false; /* Never evaluate malformed bool representation. */
    return !present || xl6CScalarSlotV1(data->base,(const unsigned char*)item+data->offset);
}
static bool value(const void* opaque,const xvalue* item)
{
    const NullableCScalarPolicy* data=opaque;
    return item && (xrtValueType(item)==XVALUE_NULL || xl6CScalarValueV1(data->base,data->identity,item));
}
static bool equivalent(const void* left,const void* right)
{
    const NullableCScalarPolicy* a=left; const NullableCScalarPolicy* b=right;
    return a->type==b->type && a->base==b->base && a->identity==b->identity;
}
static bool defaultable(const void* opaque) { (void)opaque; return true; }
static const XL6_SequenceAdmissionOpsV2 ops={retain,drop,slot,value,equivalent,defaultable,NULL,NULL};
xvalue* xl6ArrayNullableCScalarCreateV1(uint64_t identity,uint64_t element,
    const xrttype* type,const xrttype* base,uint64_t boxedIdentity)
{
    if (!identity || !element || !boxedIdentity || element==boxedIdentity ||
        !xl6CScalarTypeV1(base) || !xl6NullableCScalarTypeAbiV1(type,base)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    NullableCScalarPolicy* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(NullableCScalarPolicy){.refs=1,.base=base,.type=type,.identity=boxedIdentity,
        .offset=(1+base->Align-1)/base->Align*base->Align};
    XL6_SequenceAdmissionV3 admission={&ops,data,element,1,type->Size,type,boxedIdentity};
    xvalue* result=xl6ArrayCreateV1(identity,element,1,type->Size,type);
    if (result) {
        XL6_Sequence* owner=xl6SequenceBeginRoleV1(result,XL6_SEQUENCE_ARRAY_V1,identity,element,1,type->Size,type);
        if (!owner || !xl6SequenceAdmissionTakeV3(owner,&admission)) {
            if (owner) xl6SequenceEndV1(owner);
            xerror* error=xrtTakeError(); xrtValueRelease(result); result=NULL;
            xrtClearError(); xrtSetErrorTake(error);
        } else xl6SequenceEndV1(owner);
    }
    xl6SequenceAdmissionDropV3(&admission); return result;
}
