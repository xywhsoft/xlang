#ifndef XL6_FOREIGN_CONTEXT_H
#define XL6_FOREIGN_CONTEXT_H
#include "xl6_code_lease.h"
/* Resident execution-context slots. Windows fibers have independent native
 * stacks; neither process globals nor ordinary thread TLS represents them.
 * Keys are created once (cold OS metadata), with rollback on partial failure.
 * They contain borrowed stack pointers or integer state, never heap owners.
 * Ready is checked before publication/entry. Get/Set require a successful
 * Ready in the runtime; Get does not allocate or run an initializer. */
typedef enum XL6_ForeignContextKeyV1 {
    XL6_FOREIGN_CONTEXT_CALLBACK_TOP_V1,
    XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1,
    XL6_FOREIGN_CONTEXT_VIEW_DRAINING_V1,
    XL6_FOREIGN_CONTEXT_KEY_COUNT_V1
} XL6_ForeignContextKeyV1;
XL6_CODE_LEASE_API bool xl6ForeignContextReadyV1(void);
XL6_CODE_LEASE_API void* xl6ForeignContextGetV1(XL6_ForeignContextKeyV1);
XL6_CODE_LEASE_API bool xl6ForeignContextSetV1(XL6_ForeignContextKeyV1,void*);
/* Physical calls may end in a different nesting order, but only in their
 * original execution context. Intrusive caller stack storage; no allocation.
 * Contains validates membership before any native epilogue is allowed. */
typedef struct XL6_ForeignCallContextV1 {
    struct XL6_ForeignCallContextV1* self;
    struct XL6_ForeignCallContextV1* older;
    struct XL6_ForeignCallContextV1* newer;
    const void* owner;
} XL6_ForeignCallContextV1;
XL6_CODE_LEASE_API bool xl6ForeignContextCallEnterV1(XL6_ForeignCallContextV1*,const void*);
XL6_CODE_LEASE_API bool xl6ForeignContextCallContainsV1(const XL6_ForeignCallContextV1*);
XL6_CODE_LEASE_API bool xl6ForeignContextCallOwnsV1(const void*);
XL6_CODE_LEASE_API void xl6ForeignContextCallLeaveV1(XL6_ForeignCallContextV1*);
#endif
