#include "src/runtime/xl6_restore_class.h"
#include "src/runtime/xl6_list.h"

static bool native(const xvalue* value)
{ return xl6RestoreClassClosedProviderV1()->IsNative(value); }
static bool admits(const xvalue* value,uint64_t identity,int64_t expected)
{
    return xl6RestoreClassClosedProviderV1()->Admits(value,identity,expected);
}
static bool ownershipAdmits(xrtownershipref ref)
{
    /* Admission certifies only exact resident families, not their children.
     * In particular an opaque/active host owner behind a lease is NOT a leaf.
     * Native future/reference policies need their own explicit allowlist and
     * remain refused here until that capability is supplied by the host. */
    return xl6RestoreClassClosedProviderV1()->OwnershipAdmits(ref) || xl6ListOwnershipAdapterV1(ref);
}
const XL6_RestoreObjectProviderV1* xl6RestoreClassProviderV1(void)
{ static const XL6_RestoreObjectProviderV1 provider={native,admits,ownershipAdmits,NULL}; return &provider; }
