#include "src/runtime/xl6_restore_class.h"
#include "src/runtime/xl6_code_lease.h"

static bool native(const xvalue* value)
{ return value && xrtValueType(value)==XVALUE_OBJECT && xl6ClassOwnershipAdapterV1(xrtValueOwnership(value)); }
static bool admits(const xvalue* value,uint64_t identity,int64_t expected)
{
    int64_t tag=0;
    /* Exact existing lifetime/finalizer family BEFORE field access. Merely
     * stamping an ordinary Object with this TypeId/marker is not admission.
     * Looking up the resident adapter does not certify the object's complete
     * collector graph or authorize collection/retirement. */
    return native(value) && xrtValueTypeId(value)==identity &&
        xrtValueGetInt(xrtValueObjectGet(value,XRT_STR_LITERAL("$xlir.class.runtime-type")),&tag) && tag==expected;
}
const XL6_RestoreObjectProviderV1* xl6RestoreClassProviderV1(void)
{ static const XL6_RestoreObjectProviderV1 provider={native,admits}; return &provider; }
