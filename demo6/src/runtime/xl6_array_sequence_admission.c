#include "src/runtime/xl6_array_admission_internal.h"

static bool admitted(const void* opaque,const xvalue* value)
{
    const XL6_ArrayAdmissionData* data=opaque;
    if (!value) return false;
    if (xrtValueType(value)==XVALUE_NULL) return data->nullable;
    return xl6SequenceAdmitsArrayV1(value,data->identity);
}
xvalue* xl6ArraySequenceCreateV1(uint64_t identity,uint64_t element,uint64_t childIdentity,bool nullable)
{
    if (!identity || !element || !childIdentity ||
        (nullable ? element==childIdentity : element!=childIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    XL6_ArrayAdmissionData* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(XL6_ArrayAdmissionData){.refs=1,.nullable=nullable,.identity=childIdentity};
    return create(identity,element,data,&policyOps);
}
