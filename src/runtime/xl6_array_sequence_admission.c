#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_resident_reference_schema.h"
#include <stdlib.h>

/* Only this policy owns a recursive witness. Do not enlarge the unrelated
 * reference/nominal/callable metadata or retain a child Value/storage edge. */
typedef struct NestedPolicy {
    volatile int32_t refs;
    bool nullable;
    XL6_ResidentReferenceSchemaV1 schema;
} NestedPolicy;
typedef struct SequenceSchema {
    XL6_SequenceRoleV1 childRole;
    XL6_SequenceLayoutV1 childLayout;
    XL6_SequenceAdmissionV3 childAdmission;
} SequenceSchema;

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
    if (!refs) { data->schema.ops->Drop(data->schema.data); xrtFree(data); }
}

static bool admitted(const void* opaque,const xvalue* value)
{
    const NestedPolicy* data=opaque;
    if (!value) return false;
    if (xrtValueType(value)==XVALUE_NULL) return data->nullable;
    return data->schema.ops->Admits(data->schema.data,value);
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
    return a->nullable==b->nullable && a->schema.identity==b->schema.identity &&
        a->schema.depth==b->schema.depth && a->schema.ops==b->schema.ops &&
        (a->schema.data==b->schema.data ||
         a->schema.ops->Equivalent(a->schema.data,b->schema.data));
}
static bool sequenceEquivalent(const void* left,const void* right)
{
    const SequenceSchema* a=left; const SequenceSchema* b=right;
    const XL6_SequenceLayoutV1* x=&a->childLayout; const XL6_SequenceLayoutV1* y=&b->childLayout;
    const XL6_SequenceAdmissionV3* p=&a->childAdmission; const XL6_SequenceAdmissionV3* q=&b->childAdmission;
    return a->childRole==b->childRole &&
        x->identity==y->identity && x->elementIdentity==y->elementIdentity && x->transfer==y->transfer &&
        x->size==y->size && x->type==y->type && p->ops==q->ops &&
        p->elementIdentity==q->elementIdentity && p->transfer==q->transfer &&
        p->size==q->size && p->type==q->type && p->boxedIdentity==q->boxedIdentity && p->scalarDomain==q->scalarDomain &&
        (p->data==q->data || p->ops->Equivalent(p->data,q->data));
}
static bool defaultable(const void* opaque) { return ((const NestedPolicy*)opaque)->nullable; }
static const XL6_SequenceAdmissionOpsV2 policyOps={retain,drop,slot,admitted,equivalent,defaultable,NULL,NULL};
static void sequenceDrop(const void* opaque)
{
    SequenceSchema* schema=(SequenceSchema*)opaque;
    xl6SequenceAdmissionDropV3(&schema->childAdmission); xrtFree(schema);
}
static bool sequenceAdmits(const void* opaque,const xvalue* value)
{
    const SequenceSchema* schema=opaque;
    return xl6SequenceAdmitsSchemaV1(value,schema->childRole,&schema->childLayout,&schema->childAdmission);
}
static const XL6_ResidentReferenceSchemaOpsV1 sequenceSchemaOps={sequenceDrop,sequenceAdmits,sequenceEquivalent};
unsigned int xl6ResidentAdmissionDepthV1(const XL6_SequenceAdmissionV3* policy)
{
    return policy && policy->ops==&policyOps && policy->data
        ? ((const NestedPolicy*)policy->data)->schema.depth : 0;
}
xvalue* xl6SequenceResidentReferenceCreateTakeV1(unsigned int role,uint64_t identity,
    uint64_t element,XL6_ResidentReferenceSchemaV1* schema,bool nullable)
{
    if (!schema || !schema->ops || !schema->data || !schema->identity ||
        !schema->ops->Drop || !schema->ops->Admits || !schema->ops->Equivalent) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    XL6_ResidentReferenceSchemaV1 owned=*schema; *schema=(XL6_ResidentReferenceSchemaV1){0};
    if (!identity || !element || (role!=XL6_SEQUENCE_ARRAY_V1 && role!=XL6_SEQUENCE_STACK_V1) ||
        (nullable ? element==owned.identity : element!=owned.identity) || !owned.depth || owned.depth>64) {
        xrtSetErrorKind(owned.depth>64 ? XERR_RANGE : XERR_ARGUMENT);
        owned.ops->Drop(owned.data); return NULL;
    }
    NestedPolicy* data=xrtMalloc(sizeof(*data));
    if (!data) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        owned.ops->Drop(owned.data); return NULL;
    }
    *data=(NestedPolicy){.refs=1,.nullable=nullable,.schema=owned};
    XL6_SequenceAdmissionV3 admission={&policyOps,data,element,3,0,sizeof(xvalue*),xrtTypeValue(),0};
    return xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,identity,&admission);
}
bool xl6SequenceNestedChildCheckV1(XL6_Sequence* parent,const xvalue* child)
{
    XL6_SequenceAdmissionV3 policy={0};
    if (!xl6SequenceAdmissionCopyV3(parent,&policy)) return false;
    const NestedPolicy* nested=policy.ops==&policyOps ? policy.data : NULL;
    const SequenceSchema* sequence=nested && nested->schema.ops==&sequenceSchemaOps
        ? nested->schema.data : NULL;
    bool ok=child ? sequence && xrtValueType(child)!=XVALUE_NULL && admitted(policy.data,child) :
        policy.ops && (!sequence || sequence->childRole!=XL6_SEQUENCE_ARRAY_V1);
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
    unsigned int depth=xl6ResidentAdmissionDepthV1(&childAdmission)+1;
    if (depth>64) { xrtSetErrorKind(XERR_RANGE); goto done; }
    SequenceSchema* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    *data=(SequenceSchema){.childRole=childRole,.childLayout=childLayout,.childAdmission=childAdmission};
    childAdmission=(XL6_SequenceAdmissionV3){0};
    XL6_ResidentReferenceSchemaV1 schema={&sequenceSchemaOps,data,childLayout.identity,depth};
    result=xl6SequenceResidentReferenceCreateTakeV1(role,identity,element,&schema,nullable);
done:
    xl6SequenceAdmissionDropV3(&childAdmission);
    releasePreservingError(exemplar);
    return result;
}
