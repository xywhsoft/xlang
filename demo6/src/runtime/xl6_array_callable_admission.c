#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "src/runtime/xl6_array_admission_internal.h"

/* This is a logical signature domain for owned boxed callable slots, not a
 * closure-pointer descriptor or a certificate for arbitrary callback graphs.
 * Runtime-family identity is checked before the opaque callable payload can
 * be inspected. Boxed language closures already own their code leases. */
static bool admitted(const void* opaque,const xvalue* value)
{
    const XL6_ArrayAdmissionData* data=opaque;
    if (!value || xrtValueType(value)==XVALUE_NULL) return data->nullable;
    return xrtValueTypeId(value)==data->identity && xrtValueIsCallable(value);
}
bool xl6SequenceCallablePolicyV1(XL6_Sequence* owner,uint64_t signature)
{
    XL6_SequenceAdmissionV3 admission={0};if(!xl6SequenceAdmissionCopyV3(owner,&admission))return false;
    bool ok=signature&&admission.elementIdentity==signature&&admission.transfer==3&&
        admission.size==sizeof(xvalue*)&&admission.type==xrtTypeValue()&&admission.ops==&policyOps&&
        admission.data&&!admission.boxedIdentity;
    if(ok){const XL6_ArrayAdmissionData* data=admission.data;
        ok=!data->nullable&&data->identity==signature&&!data->rule&&!data->childRole&&!data->nominal.kind;}
    xl6SequenceAdmissionDropV3(&admission);return ok;
}
xvalue* xl6SequenceCallableCreateV1(unsigned int role,uint64_t identity,uint64_t element,
    uint64_t signature,bool nullable)
{
    if (!identity || !element || !signature ||
        (nullable ? element==signature : element!=signature)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    XL6_ArrayAdmissionData* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(XL6_ArrayAdmissionData){.refs=1,.nullable=nullable,.identity=signature};
    return create(role,identity,element,data,&policyOps);
}
