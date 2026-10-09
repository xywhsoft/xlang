#ifndef XL6_THREAD_H
#define XL6_THREAD_H
#include "src/runtime/xl6_closure.h"
struct xvalue;
struct xrtownershipref;
struct xrtownershipadapterv1;
/* Borrows an actual resident closure; its exact () -> i64 signature is an
 * IR/native caller precondition. The worker and native methods hold real pins.
 * Source code appears only behind the closure's existing residency contract,
 * never as a generated owner/drop/trace callback. Status calls preserve errors.
 * Final shell release waits outside its own gate/mutation; self-release cannot
 * join itself. The XRT wait contract is logical completion, not OS-thread join. */
XL6_CODE_LEASE_API struct xvalue* xl6ThreadCreateV1(XL6_Closure*,uint64_t);
XL6_CODE_LEASE_API bool xl6ThreadWaitV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6ThreadWaitTimeoutV1(const struct xvalue*,uint64_t,int64_t);
XL6_CODE_LEASE_API int64_t xl6ThreadExitCodeV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API int64_t xl6ThreadStateV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6ThreadStopV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6ThreadAdmitsV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ThreadOwnershipAdapterV1(struct xrtownershipref);
#endif
