#ifndef XL_CANCELLATION_RETIREMENT_H
#define XL_CANCELLATION_RETIREMENT_H
#include "xl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* An exceptional refused Unwatch keeps its real owner and callback lease in
 * an allocation-free resident record. Tokens are monotonic IDs, not pointers.
 * Snapshot does not pin rows; a concurrent successful retry may retire an ID.
 * Retry never requests cancellation or invokes an accepted callback, but may
 * wait for a callback already running elsewhere. Do not hold caller locks
 * which that callback needs. A failed retry preserves the registration right.
 * Checked status is independent of diagnostic allocation. */
typedef struct XL_CancellationRetirementV1 {
    uint64_t token;
    unsigned int errorKind;
} XL_CancellationRetirementV1;
XL_API int xlCancellationRetirementSnapshotCheckedV1(XL_CancellationRetirementV1*,size_t,size_t*);
XL_API int xlCancellationRetirementRetryCheckedV1(uint64_t);
#ifdef __cplusplus
}
#endif
#endif
