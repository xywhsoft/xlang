#include "src/runtime/xl6_nominal_admission.h"
#include "src/runtime/xl6_value_admission_internal.h"

static bool valid(const XL6_NominalAdmissionV1* schema)
{
    return schema && schema->kind>=XL6_NOMINAL_ADMIT_CLASS_TAG &&
        schema->kind<=XL6_NOMINAL_ADMIT_PROTOCOL_TAG &&
        (!schema->count || schema->identities) &&
        schema->count<=SIZE_MAX/sizeof(*schema->identities) &&
        (schema->kind==XL6_NOMINAL_ADMIT_CLASS_TAG || !schema->nonObjectIdentity);
}
bool xl6ValueAdmitsNominalV1(const xvalue* value, const XL6_NominalAdmissionV1* schema, bool nullable)
{
    if (!value || !valid(schema)) return false;
    xvaluetype tag=xrtValueType(value);
    if (tag==XVALUE_NULL) return nullable;
    uint64_t identity;
    if (schema->kind==XL6_NOMINAL_ADMIT_EXACT_ID) identity=xrtValueTypeId(value);
    else {
        if (tag!=XVALUE_OBJECT) return schema->kind==XL6_NOMINAL_ADMIT_CLASS_TAG &&
            schema->nonObjectIdentity && xrtValueTypeId(value)==schema->nonObjectIdentity;
        const xstrview key={"$xlir.class.runtime-type",sizeof("$xlir.class.runtime-type")-1};
        xvalue* item=xrtValueObjectGet(value,key);
        int64_t runtimeTag=0;
        /* The getter reports type errors. Overload admission is a pure
         * mismatch predicate: reject a malformed tag before calling it. */
        if (!item || xrtValueType(item)!=XVALUE_INT ||
            !xrtValueGetInt(item,&runtimeTag) || runtimeTag<=0) return false;
        identity=(uint64_t)runtimeTag;
    }
    if (!identity) return false;
    for (size_t i=0;i<schema->count;++i)
        if (schema->identities[i]==identity) return true;
    return false;
}
typedef struct NominalContext {
    const XL6_NominalAdmissionV1* schema;
    bool nullable;
} NominalContext;
static bool admits(const xvalue* value, const void* opaque)
{
    const NominalContext* context=opaque;
    return xl6ValueAdmitsNominalV1(value,context->schema,context->nullable);
}
xvalue* xl6CollectionCheckedNominalTakeV1(xvalue* source, int container,
    const XL6_NominalAdmissionV1* schema, bool nullable)
{
    if (!valid(schema)) return xl6AdmissionFailure(source,NULL,XERR_ARGUMENT);
    const NominalContext context={schema,nullable};
    return xl6AdmissionCollectionTake(source,container,admits,&context);
}
