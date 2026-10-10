#include "src/runtime/xl6_array_admission_internal.h"
#include "src/runtime/xl6_value_admission.h"

static bool admitted(const void* opaque,const xvalue* value)
{
    const XL6_ArrayAdmissionData* data=opaque;
    return xl6ValueAdmitsV1(value,data->rule,data->nullable,data->identity);
}
/* Empty bytes is a language default, not the empty transfer destination.
 * Only resize uses this initializer; push/copy do not allocate an empty value
 * just to replace it. The shared XRT resize owns rollback and publication. */
static bool bytesDefault(const void* opaque,ptr slot)
{
    (void)opaque;
    xvalue* value=xrtValueBytes((xbytesview){NULL,0});
    if (!value) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    *(xvalue**)slot=value; return true;
}
static const XL6_SequenceAdmissionOpsV2 bytesOps={policyRetain,policyDrop,slotAdmitted,
    admitted,equivalent,defaultable,bytesDefault,NULL};
xvalue* xl6SequenceReferenceCreateV1(unsigned int role,uint64_t identity,uint64_t element,unsigned int rule,bool nullable,uint64_t valueIdentity)
{
    if (!identity || !element || (rule!=XL6_VALUE_ADMIT_ANY && rule!=XL6_VALUE_ADMIT_BYTES) ||
        (rule==XL6_VALUE_ADMIT_BYTES && !valueIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    XL6_ArrayAdmissionData* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(XL6_ArrayAdmissionData){.refs=1,.nullable=nullable,.rule=rule,.identity=valueIdentity};
    return create(role,identity,element,data,rule==XL6_VALUE_ADMIT_BYTES && !nullable ? &bytesOps : &policyOps);
}
bool xl6ReferenceAnyAdmissionV1(const XL6_SequenceAdmissionV3* policy,uint64_t element)
{
    if (!policy) return false;
    XL6_SequenceAdmissionV3 admission=*policy;
    bool matches=element && admission.elementIdentity==element && admission.transfer==3 &&
        admission.size==sizeof(xvalue*) && admission.type==xrtTypeValue() &&
        admission.ops==&policyOps && admission.data && !admission.boxedIdentity;
    if(matches) {
        const XL6_ArrayAdmissionData* data=admission.data;
        matches=data->rule==XL6_VALUE_ADMIT_ANY && !data->nullable &&
            data->identity==element && !data->childRole && !data->nominal.kind;
    }
    return matches;
}
bool xl6SequenceReferenceAnyPolicyV1(XL6_Sequence* owner,uint64_t element)
{
    XL6_SequenceAdmissionV3 admission={0};
    if (!xl6SequenceAdmissionCopyV3(owner,&admission)) return false;
    bool matches=xl6ReferenceAnyAdmissionV1(&admission,element);
    xl6SequenceAdmissionDropV3(&admission); return matches;
}
