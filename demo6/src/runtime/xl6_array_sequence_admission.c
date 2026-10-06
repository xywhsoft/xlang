#include "src/runtime/xl6_array_admission.h"
#include <stdlib.h>

/* Only this policy owns a recursive witness. Do not enlarge the unrelated
 * reference/nominal/callable metadata or retain a child Value/storage edge. */
typedef struct NestedPolicy {
    volatile int32_t refs;
    bool nullable;
    unsigned int schemaDepth;
    XL6_SequenceRoleV1 childRole;
    XL6_SequenceLayoutV1 childLayout;
    XL6_SequenceAdmissionV3 childAdmission;
} NestedPolicy;

static bool retain(const void* opaque)
{
    NestedPolicy* data=(NestedPolicy*)opaque;
    if (!data || xrtRefRetain(&data->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void drop(const void* opaque)
{
    NestedPolicy* data=(NestedPolicy*)opaque;
    int32_t refs=xrtRefRelease(&data->refs); if (refs<0) abort();
    if (!refs) { xl6SequenceAdmissionDropV3(&data->childAdmission); xrtFree(data); }
}

static bool admitted(const void* opaque,const xvalue* value)
{
    const NestedPolicy* data=opaque;
    if (!value) return false;
    if (xrtValueType(value)==XVALUE_NULL) return data->nullable;
    return xl6SequenceAdmitsSchemaV1(value,data->childRole,&data->childLayout,&data->childAdmission);
}
static bool slot(const void* opaque,const void* item)
{
    if (!item) return false;
    const xvalue* value=*(xvalue* const*)item;
    return admitted(opaque,value ? value : xrtValueNull());
}
static bool equivalent(const void* left,const void* right)
{
    const NestedPolicy* a=left; const NestedPolicy* b=right;
    const XL6_SequenceLayoutV1* x=&a->childLayout; const XL6_SequenceLayoutV1* y=&b->childLayout;
    const XL6_SequenceAdmissionV3* p=&a->childAdmission; const XL6_SequenceAdmissionV3* q=&b->childAdmission;
    return a->nullable==b->nullable && a->schemaDepth==b->schemaDepth && a->childRole==b->childRole &&
        x->identity==y->identity && x->elementIdentity==y->elementIdentity && x->transfer==y->transfer &&
        x->size==y->size && x->type==y->type && p->ops==q->ops &&
        p->elementIdentity==q->elementIdentity && p->transfer==q->transfer &&
        p->size==q->size && p->type==q->type && p->boxedIdentity==q->boxedIdentity &&
        (p->data==q->data || p->ops->Equivalent(p->data,q->data));
}
static bool defaultable(const void* opaque) { return ((const NestedPolicy*)opaque)->nullable; }
static const XL6_SequenceAdmissionOpsV2 policyOps={retain,drop,slot,admitted,equivalent,defaultable,NULL,NULL};
bool xl6SequenceNestedChildCheckV1(XL6_Sequence* parent,const xvalue* child)
{
    XL6_SequenceAdmissionV3 policy={0};
    if (!xl6SequenceAdmissionCopyV3(parent,&policy)) return false;
    bool ok=child ? policy.ops==&policyOps && xrtValueType(child)!=XVALUE_NULL &&
        admitted(policy.data,child) : policy.ops &&
        (policy.ops!=&policyOps || ((const NestedPolicy*)policy.data)->childRole!=XL6_SEQUENCE_ARRAY_V1);
    xl6SequenceAdmissionDropV3(&policy);
    if (!ok) xrtSetErrorKind(XERR_TYPE);
    return ok;
}
static void releasePreservingError(xvalue* value)
{
    xerror* prior=xrtTakeError(); xrtValueRelease(value);
    if (prior) { xrtClearError(); xrtSetErrorTake(prior); }
}
xvalue* xl6SequenceNestedCreateTakeV2(unsigned int role,uint64_t identity,uint64_t element,
    xvalue* exemplar,bool nullable)
{
    XL6_SequenceRoleV1 childRole=0; XL6_SequenceLayoutV1 childLayout={0};
    XL6_SequenceAdmissionV3 childAdmission={0}; xvalue* result=NULL;
    /* A mechanically emitted child factory may fail first. Do not replace
     * that diagnostic while propagating its NULL through the outer recipe. */
    if (!exemplar && xrtGetError()) return NULL;
    if (!identity || !element || (role!=XL6_SEQUENCE_ARRAY_V1 && role!=XL6_SEQUENCE_STACK_V1)) {
        xrtSetErrorKind(XERR_ARGUMENT); goto done;
    }
    if (!xl6SequenceSchemaCopyV1(exemplar,&childRole,&childLayout,&childAdmission)) goto done;
    if (nullable ? element==childLayout.identity : element!=childLayout.identity) {
        xrtSetErrorKind(XERR_ARGUMENT); goto done;
    }
    unsigned int depth=childAdmission.ops==&policyOps
        ? ((const NestedPolicy*)childAdmission.data)->schemaDepth+1 : 1;
    if (depth>64) { xrtSetErrorKind(XERR_RANGE); goto done; }
    NestedPolicy* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    *data=(NestedPolicy){.refs=1,.nullable=nullable,
        .childRole=childRole,.childLayout=childLayout,.childAdmission=childAdmission,.schemaDepth=depth};
    childAdmission=(XL6_SequenceAdmissionV3){0};
    XL6_SequenceAdmissionV3 admission={&policyOps,data,element,3,sizeof(xvalue*),xrtTypeValue(),0};
    result=xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,identity,&admission);
done:
    xl6SequenceAdmissionDropV3(&childAdmission);
    releasePreservingError(exemplar);
    return result;
}
