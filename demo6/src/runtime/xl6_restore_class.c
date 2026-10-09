#include "src/runtime/xl6_restore_class.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_list.h"

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
static bool ownershipAdmits(xrtownershipref ref)
{
    /* Admission certifies only exact resident families, not their children.
     * In particular an opaque/active host owner behind a lease is NOT a leaf.
     * Native future/reference policies need their own explicit allowlist and
     * remain refused here until that capability is supplied by the host. */
    return xrtValueOwnershipAdapterV1(ref) || xl6ClassOwnershipAdapterV1(ref) ||
        xl6CodeLeaseOwnershipAdapterV1(ref) || xl6SequenceOwnershipAdapterV1(ref) ||
        xl6ListOwnershipAdapterV1(ref) || xrtValueCursorOwnershipAdapterV1(ref);
}
const XL6_RestoreObjectProviderV1* xl6RestoreClassProviderV1(void)
{ static const XL6_RestoreObjectProviderV1 provider={native,admits,ownershipAdmits}; return &provider; }
