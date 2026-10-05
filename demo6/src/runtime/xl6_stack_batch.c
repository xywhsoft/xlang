#include "src/runtime/xl6_stack_batch.h"

#define PARAMS uint64_t identity,uint64_t element,unsigned int transfer,size_t size,const xrttype* type
#define STACK_LAYOUT identity,element,transfer,size,type
#define ARRAY_LAYOUT arrayIdentity,element,transfer,size,type
static bool allocation(bool ok)
{ if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return ok; }
static bool range(int64_t value)
{
    if (value<0 || (uint64_t)value>SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    return true;
}
bool xl6StackPushBatchV1(const xvalue* stack,PARAMS,const xvalue* array,uint64_t arrayIdentity)
{
    XL6_Sequence* target=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,STACK_LAYOUT);
    if (!target) return false;
    XL6_Sequence* source=xl6SequenceBeginRoleV1(array,XL6_SEQUENCE_ARRAY_V1,ARRAY_LAYOUT);
    if (!source) { xl6SequenceEndV1(target); return false; }
    /* Validate the actual target domain before XRT allocates or publishes any
     * element. Low-level unprofiled sources may contain valid slots too; an
     * identity/physical match alone is not proof of their logical domain. */
    xtypedarray* items=xl6SequenceStorageV1(source);
    bool ok=true;
    for (size_t i=0;i<xrtTypedArrayCount(items);++i)
        if (!xl6SequenceAdmitSlotV1(target,xrtTypedArrayConstGet(items,i))) { ok=false; break; }
    if (ok) ok=allocation(xrtTypedStackPushBatch(xl6SequenceStorageV1(target),items));
    xl6SequenceEndV1(source); xl6SequenceEndV1(target); return ok;
}
bool xl6StackExtractBatchV1(const xvalue* stack,PARAMS,const xvalue* array,uint64_t arrayIdentity,
    int64_t depth,int64_t maximum,bool take)
{
    if (!range(depth) || !range(maximum)) return false;
    XL6_Sequence* source=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,STACK_LAYOUT);
    if (!source) return false;
    XL6_Sequence* target=xl6SequenceBeginRoleV1(array,XL6_SEQUENCE_ARRAY_V1,ARRAY_LAYOUT);
    if (!target) { xl6SequenceEndV1(source); return false; }
    bool ok=xl6SequenceExtractTailV1(source,target,(size_t)depth,(size_t)maximum,take);
    xl6SequenceEndV1(target); xl6SequenceEndV1(source); return ok;
}
#undef ARRAY_LAYOUT
#undef STACK_LAYOUT
#undef PARAMS
