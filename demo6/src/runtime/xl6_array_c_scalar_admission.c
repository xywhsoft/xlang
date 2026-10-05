#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_c_scalar_domain.h"
#include <stdlib.h>
#include <string.h>

/* C ABI scalars use their actual native-width XRT descriptor, not the
 * language's widened i64/u8 boxed convention. Only resident builtin singleton
 * descriptors are accepted; foreign callbacks/lifetimes are not inferred. */
typedef struct CScalarPolicy {
    volatile int32_t refs;
    const xrttype* type;
    uint64_t identity;
} CScalarPolicy;
static bool physical(const xrttype* type)
{
    return xl6CScalarTypeV1(type);
}
static bool retain(const void* opaque)
{
    CScalarPolicy* data=(CScalarPolicy*)opaque;
    if (!data || xrtRefRetain(&data->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void drop(const void* opaque)
{
    CScalarPolicy* data=(CScalarPolicy*)opaque;
    int32_t refs=xrtRefRelease(&data->refs); if (refs<0) abort();
    if (!refs) xrtFree(data);
}
static bool slot(const void* opaque,const void* item)
{
    const CScalarPolicy* data=opaque;
    return xl6CScalarSlotV1(data->type,item);
}
static bool value(const void* opaque,const xvalue* item)
{
    const CScalarPolicy* data=opaque;
    return xl6CScalarValueV1(data->type,data->identity,item);
}
static bool equivalent(const void* a,const void* b)
{
    const CScalarPolicy* left=a; const CScalarPolicy* right=b;
    return left->type==right->type && left->identity==right->identity;
}
static bool defaultable(const void* opaque) { (void)opaque; return true; }
static const XL6_SequenceAdmissionOpsV2 ops={retain,drop,slot,value,equivalent,defaultable,NULL,NULL};

xvalue* xl6ArrayCScalarCreateV1(uint64_t identity,uint64_t element,const xrttype* type,uint64_t boxedIdentity)
{
    if (!identity || !element || element!=boxedIdentity || !physical(type)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    CScalarPolicy* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(CScalarPolicy){.refs=1,.type=type,.identity=boxedIdentity};
    XL6_SequenceAdmissionV3 admission={&ops,data,element,1,type->Size,type,boxedIdentity};
    return xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_ARRAY_V1,identity,&admission);
}
