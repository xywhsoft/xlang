#include "xl6_array_admission.h"
#include <stdlib.h>

typedef struct UnionPolicy {
    volatile int32_t refs;
    uint64_t identity;
    size_t size;
    bool nullable;
} UnionPolicy;
static bool unionRetain(const void* raw)
{
    UnionPolicy* data=(UnionPolicy*)raw;
    if (data && xrtRefRetain(&data->refs)>0) return true;
    xrtSetErrorKind(XERR_STATE); return false;
}
static void unionDrop(const void* raw)
{
    UnionPolicy* data=(UnionPolicy*)raw;
    int32_t refs=xrtRefRelease(&data->refs); if (refs<0) abort();
    if (!refs) xrtFree(data);
}
static bool unionValue(const void* raw,const xvalue* item)
{
    const UnionPolicy* data=raw;
    if (!item || xrtValueType(item)==XVALUE_NULL) return data->nullable;
    /* Checking the dynamic tag first avoids GetBytes producing a diagnostic
     * on a simple domain probe. No allocation, coercion or borrowed module. */
    xbytesview bytes={0};
    return xrtValueType(item)==XVALUE_BYTES && xrtValueTypeId(item)==data->identity &&
        xrtValueGetBytes(item,&bytes) && bytes.Data && bytes.Size==data->size;
}
static bool unionSlot(const void* raw,const void* slot)
{ return slot && unionValue(raw,*(xvalue* const*)slot); }
static bool unionEquivalent(const void* a,const void* b)
{
    const UnionPolicy* left=a; const UnionPolicy* right=b;
    return left->identity==right->identity && left->size==right->size && left->nullable==right->nullable;
}
static bool unionDefaultable(const void* raw) { (void)raw; return true; }
static bool unionDefault(const void* raw,ptr output)
{
    const UnionPolicy* data=raw;
    if (data->nullable) { *(xvalue**)output=NULL; return true; }
    bytes zero=xrtCalloc(1,data->size);
    if (!zero) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    xvalue* item=xrtValueBytesTake(&zero,data->size);
    xrtFree(zero);
    if (!item) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    if (!xrtValueTypeIdBind(item,data->identity)) { xrtValueRelease(item); return false; }
    *(xvalue**)output=item; return true;
}
static const XL6_SequenceAdmissionOpsV2 unionOps={unionRetain,unionDrop,unionSlot,unionValue,
    unionEquivalent,unionDefaultable,unionDefault,NULL};
xvalue* xl6SequenceUnionCreateV1(unsigned int role,uint64_t identity,uint64_t element,
    uint64_t valueIdentity,size_t size,bool nullable)
{
    if (role!=XL6_SEQUENCE_ARRAY_V1 || !identity || !element || !valueIdentity || !size ||
        (nullable ? element==valueIdentity : element!=valueIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    UnionPolicy* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(UnionPolicy){.refs=1,.identity=valueIdentity,.size=size,.nullable=nullable};
    XL6_SequenceAdmissionV3 admission={&unionOps,data,element,3,sizeof(xvalue*),xrtTypeValue(),0};
    return xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,identity,&admission);
}
