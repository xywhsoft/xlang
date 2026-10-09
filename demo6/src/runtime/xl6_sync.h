#ifndef XL6_SYNC_H
#define XL6_SYNC_H
#include "xl6_code_lease.h"
#include "xl_sync_retirement.h"
#include <stddef.h>
struct xvalue;
struct xrtownershipref;
struct xrtownershipadapterv1;
enum { XL6_SYNC_MUTEX_V1=1, XL6_SYNC_COND_V1=2, XL6_SYNC_RWLOCK_V1=3 };
/* Resident reference owners. Bool methods preserve the incoming error and
 * never publish normal status failure. All native calls have activity pins;
 * metadata gates and world mutations end BEFORE blocking native calls. */
XL6_CODE_LEASE_API struct xvalue* xl6MutexCreateV1(uint64_t);
XL6_CODE_LEASE_API bool xl6MutexLockV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6MutexTryLockV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6MutexUnlockV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API struct xvalue* xl6CondCreateV1(uint64_t);
XL6_CODE_LEASE_API bool xl6CondWaitV1(const struct xvalue*,uint64_t,const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6CondWaitTimeoutV1(const struct xvalue*,uint64_t,const struct xvalue*,uint64_t,int64_t);
XL6_CODE_LEASE_API bool xl6CondSignalV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6CondBroadcastV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API struct xvalue* xl6RWLockCreateV1(uint64_t);
XL6_CODE_LEASE_API bool xl6RWLockReadV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6RWLockTryReadV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6RWLockReadUnlockV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6RWLockWriteV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6RWLockTryWriteV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6RWLockWriteUnlockV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6RWLockDowngradeV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6RWLockUpgradeV1(const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API bool xl6SyncAdmitsV1(const struct xvalue*,uint64_t,unsigned int);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6SyncOwnershipAdapterV1(struct xrtownershipref);

/* Failed retirement holds a REAL registry reference, never a lost raw
 * pointer or an automatic unlock. Snapshot is read-only and allocation-free;
 * Token identifies the record, not a dereferenceable address. Acquire creates
 * a genuine owned recovery handle; only explicit unlock is permitted on a
 * pending/cleared owner. Retry refuses live activity, keeps the record on
 * failure, and removes its reference only AFTER successful native Unit.
 * A mutex must be unlocked on its acquiring native thread. No forced unlock,
 * thread-id substitution, generated callback or source-module data is kept. */
typedef XL_SyncRetirementV1 XL6_SyncRetirementV1;
XL6_CODE_LEASE_API size_t xl6SyncRetirementSnapshotV1(XL6_SyncRetirementV1*,size_t);
XL6_CODE_LEASE_API struct xvalue* xl6SyncRetirementAcquireV1(uint64_t);
XL6_CODE_LEASE_API bool xl6SyncRetirementRetryV1(uint64_t);
#endif
