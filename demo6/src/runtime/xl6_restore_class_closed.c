#include "src/runtime/xl6_restore_class.h"
#include "src/runtime/xl6_ownership_read.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/runtime/xl6_sequence.h"

static bool native(const xvalue* value)
{ return value && xrtValueType(value)==XVALUE_OBJECT && xl6ClassOwnershipAdapterV1(xrtValueOwnership(value)); }
static bool admits(const xvalue* value,uint64_t identity,int64_t expected)
{
    int64_t tag=0;
    /* Exact resident lifetime BEFORE field access, not a forged TypeId/tag.
     * This is a physical family check, not a complete graph certificate. */
    return native(value) && xrtValueTypeId(value)==identity &&
        xrtValueGetInt(xrtValueObjectGet(value,XRT_STR_LITERAL("$xlir.class.runtime-type")),&tag) && tag==expected;
}
static bool ownershipAdmits(xrtownershipref ref)
{
    /* Every transitive participant still passes admission before Count/Trace.
     * An opaque code owner is not a leaf; only the independent host read
     * boundary can authorize it. No optional List/Future/resource algorithms
     * are pulled into a closed class/scalar/native-Array restoration. */
    return xrtValueOwnershipAdapterV1(ref) || xl6ClassOwnershipAdapterV1(ref) ||
        xl6CodeLeaseOwnershipAdapterV1(ref) || xl6SequenceOwnershipAdapterV1(ref) ||
        xrtValueCursorOwnershipAdapterV1(ref);
}
const XL6_RestoreObjectProviderV1* xl6RestoreClassClosedProviderV1(void)
{ static const XL6_RestoreObjectProviderV1 provider={native,admits,ownershipAdmits}; return &provider; }
