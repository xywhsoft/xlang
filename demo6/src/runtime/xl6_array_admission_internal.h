#ifndef XL6_ARRAY_ADMISSION_INTERNAL_H
#define XL6_ARRAY_ADMISSION_INTERNAL_H
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include <string.h>
#include <stdlib.h>

/* No Value edges, module callbacks or borrowed identity spans. The two
 * selected components use their own immutable resident ops/predicate. */
typedef struct XL6_ArrayAdmissionData {
    volatile int32_t refs;
    bool nullable;
    unsigned int rule;
    uint64_t identity;
    XL6_NominalAdmissionV1 nominal;
    uint64_t identities[];
} XL6_ArrayAdmissionData;
static bool admitted(const void*,const xvalue*);
static bool policyRetain(const void* opaque)
{
    XL6_ArrayAdmissionData* data=(XL6_ArrayAdmissionData*)opaque;
    if (!data || xrtRefRetain(&data->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void policyDrop(const void* opaque)
{
    XL6_ArrayAdmissionData* data=(XL6_ArrayAdmissionData*)opaque;
    int32_t refs=xrtRefRelease(&data->refs); if (refs<0) abort();
    if (!refs) xrtFree(data);
}
static bool slotAdmitted(const void* opaque,const void* slot)
{
    if (!slot) return false;
    const xvalue* value=*(xvalue* const*)slot;
    return admitted(opaque,value ? value : xrtValueNull());
}
static bool equivalent(const void* a,const void* b)
{
    const XL6_ArrayAdmissionData* left=a; const XL6_ArrayAdmissionData* right=b;
    return left->nullable==right->nullable && left->rule==right->rule && left->identity==right->identity &&
        left->nominal.kind==right->nominal.kind && left->nominal.nonObjectIdentity==right->nominal.nonObjectIdentity &&
        left->nominal.count==right->nominal.count && (!left->nominal.count ||
        !memcmp(left->nominal.identities,right->nominal.identities,left->nominal.count*sizeof(uint64_t)));
}
static bool defaultable(const void* opaque)
{
    const XL6_ArrayAdmissionData* data=opaque;
    return data->nullable || data->rule==XL6_VALUE_ADMIT_ANY || data->rule==XL6_VALUE_ADMIT_BYTES;
}
static const XL6_SequenceAdmissionOpsV1 policyOps={policyRetain,policyDrop,slotAdmitted,admitted,equivalent,defaultable,NULL};
static xvalue* create(uint64_t identity,uint64_t element,XL6_ArrayAdmissionData* data,const XL6_SequenceAdmissionOpsV1* ops)
{
    XL6_SequenceAdmissionV1 admission={ops,data,element,3,sizeof(xvalue*),xrtTypeValue()};
    xvalue* value=xl6ArrayCreateV1(identity,element,3,sizeof(xvalue*),xrtTypeValue());
    if (value) {
        XL6_Sequence* owner=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,
            identity,element,3,sizeof(xvalue*),xrtTypeValue());
        if (!owner || !xl6SequenceAdmissionTakeV1(owner,&admission)) {
            if (owner) xl6SequenceEndV1(owner);
            xerror* error=xrtTakeError(); xrtValueRelease(value); value=NULL;
            xrtClearError(); xrtSetErrorTake(error);
        } else xl6SequenceEndV1(owner);
    }
    xl6SequenceAdmissionDropV1(&admission); return value;
}
#endif
