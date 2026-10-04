#include "src/runtime/xl6_array_admission_internal.h"
#include "src/runtime/xl6_nominal_admission.h"
#include <limits.h>

static bool admitted(const void* opaque,const xvalue* value)
{
    const XL6_ArrayAdmissionData* data=opaque;
    return xl6ValueAdmitsNominalV1(value,&data->nominal,data->nullable);
}
xvalue* xl6ArrayNominalCreateV2(uint64_t identity,uint64_t element,const XL6_NominalAdmissionV1* schema,bool nullable,uint64_t valueIdentity)
{
    if (!identity || !element || !valueIdentity || !schema || schema->kind<XL6_NOMINAL_ADMIT_CLASS_TAG ||
        schema->kind>XL6_NOMINAL_ADMIT_PROTOCOL_TAG || (schema->count && !schema->identities) ||
        schema->count>(SIZE_MAX-sizeof(XL6_ArrayAdmissionData))/sizeof(uint64_t) ||
        (schema->kind!=XL6_NOMINAL_ADMIT_CLASS_TAG && schema->nonObjectIdentity) ||
        (schema->nonObjectIdentity && schema->nonObjectIdentity!=valueIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    for (size_t i=0;i<schema->count;++i) {
        if (!schema->identities[i] || (schema->kind!=XL6_NOMINAL_ADMIT_EXACT_ID && schema->identities[i]>INT64_MAX)) {
            xrtSetErrorKind(XERR_ARGUMENT); return NULL;
        }
    }
    XL6_ArrayAdmissionData* data=xrtMalloc(sizeof(*data)+schema->count*sizeof(uint64_t));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(XL6_ArrayAdmissionData){.refs=1,.nullable=nullable,.identity=valueIdentity,.nominal=*schema};
    data->nominal.identities=data->identities;
    if (schema->count) memcpy(data->identities,schema->identities,schema->count*sizeof(uint64_t));
    return create(identity,element,data);
}
