#ifndef XL6_FOREIGN_SCOPE_VIEW_H
#define XL6_FOREIGN_SCOPE_VIEW_H
#include "xl6_foreign_view.h"

/* Non-owning callback value. Never convert address back to a pointer before
 * resolving lifetime in the CURRENT execution context. Copies do not retain
 * payloads, contexts or code. Zero is the nullable sentinel. These are not
 * persistence records and native publishers cannot extend their validity. */
typedef struct XL6_ForeignScopeViewV1 {
    uintptr_t address;
    uint64_t lifetime;
} XL6_ForeignScopeViewV1;

/* Intrusive caller stack storage; never copy a live registration. Begin runs
 * inside an admitted native callback. End must precede that callback's Leave.
 * Native owner-call teardown also expires registrations before any cleanup.
 * IDs are unique within the canonical resident runtime, monotonically
 * allocated and never wrap/recycle. A NULL owner explicitly selects a
 * rootless callback epoch; it never grants ownership of an unrelated object. */
typedef enum XL6_ForeignScopeOwnerV2 {
    XL6_FOREIGN_SCOPE_NONE_V2=0, XL6_FOREIGN_SCOPE_ROOT_V2,
    XL6_FOREIGN_SCOPE_ROOTLESS_V2, XL6_FOREIGN_SCOPE_EXPIRED_V2
} XL6_ForeignScopeOwnerV2;
typedef struct XL6_ForeignScopeLifetimeV2 {
    struct XL6_ForeignScopeLifetimeV2* self;
    struct XL6_ForeignScopeLifetimeV2* older;
    struct XL6_ForeignScopeLifetimeV2* newer;
    const XL6_ForeignViewCallV1* ownerCall;
    uint64_t identity;
    XL6_ForeignScopeOwnerV2 ownerKind;
} XL6_ForeignScopeLifetimeV2;

XL6_CODE_LEASE_API bool xl6ForeignScopeBeginV2(XL6_ForeignScopeLifetimeV2*,const XL6_ForeignViewCallV1*);
XL6_CODE_LEASE_API bool xl6ForeignScopeEndV2(XL6_ForeignScopeLifetimeV2*);
XL6_CODE_LEASE_API bool xl6ForeignScopeViewMakeV2(const XL6_ForeignScopeLifetimeV2*,const void*,XL6_ForeignScopeViewV1*);
/* A derived view shares its source's exact callback lifetime. NULL raw is a
 * nullable result, but still requires a live nonnull source. */
XL6_CODE_LEASE_API bool xl6ForeignScopeViewDeriveV2(XL6_ForeignScopeViewV1,const void*,XL6_ForeignScopeViewV1*);
/* On failure outputs remain unchanged. owner, if requested, is borrowed only
 * until the enclosing native callback returns. No acquisition or allocation. */
XL6_CODE_LEASE_API bool xl6ForeignScopeViewGetV2(XL6_ForeignScopeViewV1,void**,XL6_ForeignView**);
XL6_CODE_LEASE_API bool xl6ForeignScopeViewAliveV2(XL6_ForeignScopeViewV1);
/* Runtime-only epilogue; no user code, allocation or pointer conversion. */
XL6_CODE_LEASE_API void xl6ForeignScopeExpireOwnerV2(const XL6_ForeignViewCallV1*);
#endif
