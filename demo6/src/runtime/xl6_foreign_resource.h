#ifndef XL6_FOREIGN_RESOURCE_H
#define XL6_FOREIGN_RESOURCE_H
#include "xl6_text_value.h"
#include "xl6_ownership_catalog.h"

/* Explicit leaf-payload contract: raw owns no hidden language/async graph
 * edges. The callable is an owned, traced dependency, never a raw callback.
 * On success ownership of raw transfers; on failure the caller still owns it.
 * Releaser accepts one point and returns no values. It executes once, before
 * graph clearing, while its captures and code are alive. NULL raw is refused.
 * Returned objects are private resource tokens, not mutable data objects. */
XL6_TEXT_API xvalue* xl6ForeignResourceCreateV1(void* raw,const xvalue* releaser);
XL6_TEXT_API void* xl6ForeignResourcePointerV1(const xvalue* token);
XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainForeignResourceV1Get(void);
#endif
