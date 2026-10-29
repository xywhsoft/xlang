#include "src/runtime/xl6_array_admission_internal.h"
#include "src/runtime/xl6_error_value.h"

static bool admitted(const void* opaque,const xvalue* value)
{
    const XL6_ArrayAdmissionData* data=opaque;
    if (!value || xrtValueType(value)==XVALUE_NULL) return data->nullable;
    return xrtValueTypeId(value)==data->identity && xl6ErrorValueBorrowV1(value)!=NULL;
}
xvalue* xl6SequenceErrorCreateV1(unsigned int role,uint64_t identity,uint64_t element,
    uint64_t base,bool nullable)
{
    if (role!=XL6_SEQUENCE_ARRAY_V1 || !identity || !element || !base ||
        (nullable ? element==base : element!=base)) {
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    XL6_ArrayAdmissionData* data=xrtMalloc(sizeof(*data));
    if (!data) {if(!xrtGetError())xrtSetErrorKind(XERR_MEMORY);return NULL;}
    *data=(XL6_ArrayAdmissionData){.refs=1,.nullable=nullable,.identity=element};
    return create(role,identity,element,data,&policyOps);
}
