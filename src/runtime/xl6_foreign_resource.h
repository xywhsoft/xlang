#ifndef XL6_FOREIGN_RESOURCE_H
#define XL6_FOREIGN_RESOURCE_H
#include "xl6_text_value.h"
#include "xl6_ownership_catalog.h"

/* Explicit leaf-payload contract: raw owns no hidden language/async graph
 * edges. The callable is an owned, traced dependency, never a raw callback.
 * On success ownership of raw transfers; on failure the caller still owns it.
 * Releaser must carry a verified language point-action contract or an exact
 * native XRT signature (one by-value pointer, no returns or variadics).
 * It executes once, before
 * graph clearing, while its captures and code are alive. NULL raw is refused.
 * Opaque tokens clone by sharing the one finalization owner. They are private
 * resource handles, not mutable data objects or independently cloned raw data. */
XL6_TEXT_API xvalue* xl6ForeignResourceCreateV1(void* raw,const xvalue* releaser);
XL6_TEXT_API void* xl6ForeignResourcePointerV1(const xvalue* token);
XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainForeignResourceV1Get(void);
#endif
