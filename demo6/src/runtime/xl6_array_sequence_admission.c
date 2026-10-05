#include "src/runtime/xl6_array_admission_internal.h"

static bool admitted(const void* opaque,const xvalue* value)
{
    const XL6_ArrayAdmissionData* data=opaque;
    if (!value) return false;
    if (xrtValueType(value)==XVALUE_NULL) return data->nullable;
    const XL6_SequenceLayoutV1* child=&data->childLayout;
    return xl6SequenceAdmitsRoleV1(value,data->childRole,child->identity,child->elementIdentity,
        child->transfer,child->size,child->type);
}
xvalue* xl6SequenceNestedCreateV1(unsigned int role,uint64_t identity,uint64_t element,
    unsigned int childRole,uint64_t childIdentity,uint64_t childElement,unsigned int childTransfer,
    size_t childSize,const xrttype* childType,bool nullable)
{
    if (!identity || !element || !childIdentity ||
        (nullable ? element==childIdentity : element!=childIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    if ((childRole!=XL6_SEQUENCE_ARRAY_V1 && childRole!=XL6_SEQUENCE_STACK_V1) ||
        !xl6SequenceTupleV1(childIdentity,childElement,childTransfer,childSize,childType)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT);
        return NULL;
    }
    XL6_ArrayAdmissionData* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(XL6_ArrayAdmissionData){.refs=1,.nullable=nullable,.identity=childIdentity,
        .childRole=(XL6_SequenceRoleV1)childRole,
        .childLayout={childIdentity,childElement,childTransfer,childSize,childType}};
    return create(role,identity,element,data,&policyOps);
}
