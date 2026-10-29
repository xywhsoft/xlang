#ifndef XL_SYNC_RETIREMENT_H
#define XL_SYNC_RETIREMENT_H
#include "xl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Failed native cleanup retains a real resident reference. A token is not
 * an address. Snapshots are coherent per call, but do not pin each listed
 * record: concurrent successful retries may make a token stale. */
typedef struct XL_SyncRetirementV1 {
    uint64_t token, identity;
    unsigned int kind, errorKind;
} XL_SyncRetirementV1;
enum { XL_SYNC_MUTEX_V1=1, XL_SYNC_COND_V1=2, XL_SYNC_RWLOCK_V1=3, XL_SYNC_EVENT_V1=4 };
enum { XL_SYNC_UNLOCK_MUTEX_V1=1, XL_SYNC_UNLOCK_READ_V1=2, XL_SYNC_UNLOCK_WRITE_V1=3 };

/* Every Checked call returns 1/0 independently of diagnostic availability.
 * Existing escaping errors remain primary. An operation can return success
 * while that prior error is still present; failure never erases it. With no
 * prior outcome, a failed call publishes its own diagnostic when available.
 * Snapshot allocates nothing and returns the total (possibly > capacity).
 * NULL rows requires capacity=0. Output total/handle is cleared on failure.
 * Acquire returns one genuine owned handle; release it with xrtValueRelease.
 * It cannot reacquire locks, or be admitted as a fresh language resource.
 * Unlock is EXPLICIT and runs on the caller's native thread. Mutex unlock
 * must run on its original locking thread; wrong-thread failure retains the
 * record. Mode must match mutex/read/write ownership. No forced/automatic
 * unlock or inferred mode exists. Retry removes a record only after native
 * Unit succeeds. A live acquired handle then remains safe but inoperative.
 * These operations remain resident after the source module is unloaded.
 * They do not acknowledge a separately failed checked module finalizer: its
 * owner must retry that finalizer/close after repairing the native record. */
XL_API int xlSyncRetirementSnapshotCheckedV1(XL_SyncRetirementV1*,size_t,size_t*);
XL_API int xlSyncRetirementAcquireCheckedV1(uint64_t,xvalue**);
XL_API int xlSyncRetirementUnlockCheckedV1(const xvalue*,unsigned int);
XL_API int xlSyncRetirementRetryCheckedV1(uint64_t);
#ifdef __cplusplus
}
#endif
#endif
