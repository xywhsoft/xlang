#include "src/runtime/xl6_array_admission_internal.h"
#include "src/runtime/xl6_value_admission.h"

static bool admitted(const void* opaque,const xvalue* value)
{
    const XL6_ArrayAdmissionData* data=opaque;
    return xl6ValueAdmitsV1(value,data->rule,data->nullable,data->identity);
}
xvalue* xl6ArrayReferenceCreateV2(uint64_t identity,uint64_t element,unsigned int rule,bool nullable,uint64_t valueIdentity)
{
    if (!identity || !element || (rule!=XL6_VALUE_ADMIT_ANY && rule!=XL6_VALUE_ADMIT_BYTES) ||
        (rule==XL6_VALUE_ADMIT_BYTES && !valueIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    XL6_ArrayAdmissionData* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(XL6_ArrayAdmissionData){.refs=1,.nullable=nullable,.rule=rule,.identity=valueIdentity};
    return create(identity,element,data);
}
