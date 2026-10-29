/* This private declaration/public generated header must be independently
 * usable with an incomplete xrtownershipref in strict C11 and C++17. */
#include "__xl_runtime/xl6_ownership_read_abi.h"
#if defined(XL6_READ_BINDING_HEADER)
#include XL6_READ_BINDING_HEADER
#endif
#include <stddef.h>
static XL6_OwnershipReadBoundaryV1 boundary;
int main(void) { return boundary.size != 0 || sizeof(XL6_OwnershipReadScopeV1) < sizeof(void*); }
