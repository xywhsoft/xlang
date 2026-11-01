#ifndef XL6_FOREIGN_CALLBACK_SCOPE_H
#define XL6_FOREIGN_CALLBACK_SCOPE_H
#include "xl6_code_lease.h"
#include "xl6_foreign_context.h"
#include <stdint.h>

/* Native-to-language execution marker. Caller independently owns code and
 * userdata residency. Caller-owned zero storage, same thread, strict LIFO;
 * never copy a live scope. Enter/Leave allocate nothing and do not drain any
 * resource queue: native caller code remains on the C stack after Leave.
 * Windows state is fiber-local; POSIX state is thread-local through OS keys.
 * First use creates OS metadata, not an owning language allocation. */
typedef struct XL6_ForeignCallbackScopeV1 {
    struct XL6_ForeignCallbackScopeV1* self;
    struct XL6_ForeignCallbackScopeV1* previous;
    uint64_t thread;
} XL6_ForeignCallbackScopeV1;
XL6_CODE_LEASE_API bool xl6ForeignCallbackEnterV1(XL6_ForeignCallbackScopeV1*);
XL6_CODE_LEASE_API bool xl6ForeignCallbackLeaveV1(XL6_ForeignCallbackScopeV1*);
XL6_CODE_LEASE_API bool xl6ForeignCallbackActiveV1(void);
#endif
