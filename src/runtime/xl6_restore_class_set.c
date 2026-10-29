#include "xl6_restore_class.h"
#include "xl6_set.h"
static bool native(const xvalue* value) { return xl6RestoreClassClosedProviderV1()->IsNative(value); }
static bool admits(const xvalue* value,uint64_t identity,int64_t expected)
{ return xl6RestoreClassClosedProviderV1()->Admits(value,identity,expected); }
static bool ownership(xrtownershipref ref)
{ return xl6RestoreClassClosedProviderV1()->OwnershipAdmits(ref) || xl6SetOwnershipAdapterV1(ref); }
const XL6_RestoreObjectProviderV1* xl6RestoreClassSetProviderV1(void)
{ static const XL6_RestoreObjectProviderV1 provider={native,admits,ownership,NULL}; return &provider; }
