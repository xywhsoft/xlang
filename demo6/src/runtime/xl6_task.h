#ifndef XL6_TASK_H
#define XL6_TASK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "xl6_code_lease.h"
#include "xl6_future_payload.h"
#include "xl6_capture.h"
#include "xl6_capture_v2.h"
struct xcancel;
struct xfuture;
struct xrtownershipref;
struct xtaskpool;
struct xasyncfile;
struct xfileoptions;
struct xfile_impl;
struct xbytesview;

/* Private resident task carriers. Generated callbacks do not depend on the
 * executor's xtaskargs layout or manufacture a lifetime trampoline. */
typedef enum XL6_TaskOutcome {
    XL6_TASK_SUCCESS = 0, XL6_TASK_FAILED = 1, XL6_TASK_CANCELLED = 2
} XL6_TaskOutcome;
typedef struct XL6_TaskResult {
    void* Value;
    void (*Destroy)(void*, void*);
    void* DestroyData;
} XL6_TaskResult;
typedef XL6_TaskOutcome (*XL6_TaskRun)(struct xcancel*, void*, XL6_TaskResult*);
typedef void (*XL6_TaskDrop)(void*, void*);
typedef struct XL6_TaskPool XL6_TaskPool;
/* A stack submission credit, not a language-visible executor handle. Begin
 * protects lazy native-pool creation and every native admission/rollback tail.
 * Never copy a live scope; End must run on the entering thread. */
typedef struct XL6_TaskNativeScopeV1 {
    struct XL6_TaskNativeScopeV1 *previous, *next;
    XL6_TaskPool* pool;
    uint64_t thread;
    struct xtaskpool* native;
} XL6_TaskNativeScopeV1;
XL6_CODE_LEASE_API bool xl6TaskNativeBeginV1(XL6_TaskPool*, XL6_TaskNativeScopeV1*);
XL6_CODE_LEASE_API void xl6TaskNativeEndV1(XL6_TaskNativeScopeV1*);
/* Intrusive resident-resource registration. Context carries one registry owner;
 * Close is nonblocking and Drop consumes that owner. Both procedures must be
 * resident, not generated callbacks. A live registry rejects graph inspection.
 * Init is allocation-free under an existing native submission scope; Release
 * drops the shell reference only after Remove or retirement detached the node. */
typedef struct XL6_TaskResourceV1 {
    struct XL6_TaskResourceV1 *previous, *next;
    XL6_TaskPool* pool;
    void* context;
    void (*Close)(void*);
    void (*Drop)(void*);
    bool registered;
} XL6_TaskResourceV1;
XL6_CODE_LEASE_API void xl6TaskResourceInitV1(XL6_TaskResourceV1*,const XL6_TaskNativeScopeV1*,void*,void (*)(void*),void (*)(void*));
XL6_CODE_LEASE_API bool xl6TaskResourceRemoveV1(XL6_TaskResourceV1*);
XL6_CODE_LEASE_API void xl6TaskResourceReleaseV1(XL6_TaskResourceV1*);
/* Resident asynchronous file shells are registered with their module pool.
 * Quiescence closes each native file before joining the pool's resource tails;
 * language globals may release their closed shells afterward. No generated
 * code pointer or borrowed data is retained by this service. */
typedef struct XL6_AsyncFile XL6_AsyncFile;
XL6_CODE_LEASE_API XL6_AsyncFile* xl6AsyncFileOpenV1(XL6_TaskPool*, const char*, const struct xfileoptions*);
XL6_CODE_LEASE_API XL6_AsyncFile* xl6AsyncFileAdoptV1(XL6_TaskPool*, struct xfile_impl*);
XL6_CODE_LEASE_API void xl6AsyncFileDestroyV1(XL6_AsyncFile*);
XL6_CODE_LEASE_API struct xfuture* xl6AsyncFileCloseV1(XL6_AsyncFile*);
XL6_CODE_LEASE_API struct xfuture* xl6AsyncFileReadV1(XL6_AsyncFile*, uint64_t, size_t);
XL6_CODE_LEASE_API struct xfuture* xl6AsyncFileWriteV1(XL6_AsyncFile*, uint64_t, const unsigned char*, size_t);
XL6_CODE_LEASE_API struct xfuture* xl6AsyncFileFlushV1(XL6_AsyncFile*);
XL6_CODE_LEASE_API struct xfuture* xl6AsyncFileSizeV1(XL6_AsyncFile*);
XL6_CODE_LEASE_API struct xfuture* xl6AsyncFileResizeV1(XL6_AsyncFile*, uint64_t);
XL6_CODE_LEASE_API uint32_t xl6AsyncFileFlagsV1(const XL6_AsyncFile*);
XL6_CODE_LEASE_API bool xl6AsyncFileClosedV1(const XL6_AsyncFile*);
typedef struct XL6_TaskPoolConfig {
    uint32_t threads;
    size_t queueLimit, stackSize;
} XL6_TaskPoolConfig;
typedef struct XL6_TaskRuntimeV1 {
    /* NULL/all-zero configuration preserves the executor defaults. */
    XL6_TaskPool* (*PoolCreate)(const XL6_TaskPoolConfig*);
    /* Close admission, join every accepted worker and notification tail, but
     * retain the resource shell through module global destruction. Calls from
     * this pool's submitting/running/cleaning callbacks fail without waiting
     * on themselves; an external caller can retry. No join holds mutation. */
    bool (*PoolQuiesce)(XL6_TaskPool*);
    bool (*PoolDestroy)(XL6_TaskPool*);
    bool (*PoolIsQuiescent)(const XL6_TaskPool*);
    bool (*PoolIsClosing)(const XL6_TaskPool*);

    /* One allocation contains the resident header and aligned producer data;
     * Ownership reports that ONE physical node. New task activations acquire
     * a new code admission, never inherit closed root admission. The entering
     * producer must independently remain resident through allocation/failure.
     * Initialize all owning slots before any fallible capture, and use Discard
     * for prefix rollback. Drop cleans the payload only; it must NOT free data.
     * Trace lists precisely those slots, not borrowed coordination pointers.
     * Private, publishing, executing and retiring frames reject inspection. */
    void* (*Allocate)(const XL6_CodeOwner*, size_t, XL6_TaskRun, XL6_TaskDrop, XL6_CodeContextTrace);
    void (*Discard)(void*);
    struct xrtownershipref (*Ownership)(const void*);

    /* On NULL, caller still owns its private frame and must Discard it. On
     * success, frame ownership is consumed, including synchronous cancellation.
     * Every bridge allocation precedes executor admission. Language mode maps
     * error kinds to Future outcomes and forwards owned payloads without copy;
     * raw task.spawn keeps its already-verified task outcome semantics.
     * Result destructors/traces must be resident for the RESULT lifetime; the
     * task's code lease cannot serve as a hidden pin for an escaped result.
     * No resource gate or ownership mutation spans user callbacks or Submit. */
    struct xfuture* (*Submit)(XL6_TaskPool*, void*, struct xcancel*, XL6_FuturePayloadTrace, bool languageOutcome);
} XL6_TaskRuntimeV1;
XL6_CODE_LEASE_API const XL6_TaskRuntimeV1* xl6TaskRuntimeV1Get(void);
/* Additive control ABI for explicit pools. V1 execution/frame layout is not
 * extended or reinterpreted. Close stops new wrapper admission; reservations
 * acquired earlier keep their right to settle. Cancel also closes admission
 * and reaches a native pool even if its lazy creation is still in flight.
 * Wait requires Close/Cancel first, covers those reservation and cleanup tails,
 * and rejects the pool's own callbacks before waiting. No gate/mutation spans
 * an arbitrary callback, condition wait or native join. */
typedef enum XL6_TaskPoolWaitStatusV1 {
    XL6_TASK_POOL_READY_V1=0,
    XL6_TASK_POOL_TIMEOUT_V1=1,
    XL6_TASK_POOL_CANCELLED_V1=2
} XL6_TaskPoolWaitStatusV1;
typedef struct XL6_TaskPoolStatsV1 {
    size_t size;
    uint64_t threads,queueLimit,queued,running;
    uint64_t submitted,completed,succeeded,failed,cancelled,rejected;
    bool initialized,closed,cancelling;
} XL6_TaskPoolStatsV1;
typedef struct XL6_TaskPoolControlV1 {
    size_t size;
    XL6_TaskPool* (*Create)(const XL6_TaskPoolConfig*);
    bool (*Start)(XL6_TaskPool*);
    bool (*Close)(XL6_TaskPool*);
    bool (*Cancel)(XL6_TaskPool*);
    /* False is an operational failure, independently of error allocation.
     * True reports READY/TIMEOUT/CANCELLED separately. Output starts TIMEOUT
     * even on failure; no partial stats are returned. -1 waits indefinitely,
     * 0 polls, positive milliseconds are finite and checked for overflow.
     * Until is an absolute finite pinned microsecond deadline, never MAX. */
    bool (*WaitFor)(XL6_TaskPool*,int64_t,struct xcancel*,XL6_TaskPoolWaitStatusV1*);
    bool (*WaitUntil)(XL6_TaskPool*,uint64_t,struct xcancel*,XL6_TaskPoolWaitStatusV1*);
    /* Caller supplies size==sizeof(Stats). Never creates a native pool;
     * initialized=false denotes no actual worker/configuration yet. Native
     * counters come from one Get, not an inferred sum of Future states.
     * Successful quiescence retains the final native counters for later reads.
     * Admission flags and counters are not one cross-provider transaction. */
    bool (*Snapshot)(XL6_TaskPool*,XL6_TaskPoolStatsV1*);
    bool (*Quiesce)(XL6_TaskPool*);
    XL6_ModulePrepareResult (*TryQuiesce)(XL6_TaskPool*);
    bool (*Destroy)(XL6_TaskPool*);
} XL6_TaskPoolControlV1;
XL6_CODE_LEASE_API const XL6_TaskPoolControlV1* xl6TaskPoolControlV1Get(void);
/* Closed capture storage for verified callback activations. V1 allocation is
 * retained for other frame families until their own complete layout migration;
 * V2 does not certify the task pool, Watch or active worker as a graph leaf. */
typedef struct XL6_TaskRuntimeV2 {
    void* (*Allocate)(const XL6_CodeOwner*, size_t, XL6_TaskRun, const XL6_CaptureLayoutV1*);
    void (*Cleanup)(void*);
} XL6_TaskRuntimeV2;
XL6_CODE_LEASE_API const XL6_TaskRuntimeV2* xl6TaskRuntimeV2Get(void);
/* Separate exact native-leaf layout. The physical task, executor, producer and
 * code-owner protocols are unchanged; V1 metadata is never reinterpreted. */
typedef struct XL6_TaskRuntimeV3 {
    void* (*Allocate)(const XL6_CodeOwner*,size_t,XL6_TaskRun,const XL6_CaptureLayoutV2*);
    void (*Cleanup)(void*);
} XL6_TaskRuntimeV3;
XL6_CODE_LEASE_API const XL6_TaskRuntimeV3* xl6TaskRuntimeV3Get(void);
/* Nonblocking retirement preparation, additive to the unchanged V1 table.
 * Close module admission without cancelling accepted work. BUSY does not join
 * active submitters/workers/callbacks and preserves the diagnostic. Only after
 * native idle includes all finalizer tails may it join idle worker threads.
 * The caller holds the pool/code alive, outside ownership admission. */
XL6_CODE_LEASE_API XL6_ModulePrepareResult xl6TaskPoolTryQuiesceV1(XL6_TaskPool*);
/* Additive closed result publication, retaining the original frame/pool ABI.
 * Verified AsyncResultAbi selects kind before submission. The executor checks
 * the returned payload's exact resident Drop before publishing certification.
 * This does not authorize collecting pending task/Watch/frame callbacks. */
XL6_CODE_LEASE_API struct xfuture* xl6TaskSubmitPayloadV1(XL6_TaskPool*, void*,
    struct xcancel*, XL6_FuturePayloadKind, bool languageOutcome);
struct xfuturepayloadownershipv1;
/* Trusted native counterpart of the closed kind API. The immutable policy and
 * its actual Drop/Trace addresses must reside in the embedding runtime through
 * final physical Future destruction. No module-local callback is admitted by
 * this API. It uses the same executor admission, exact returned Drop identity,
 * stable producer, Watch and language outcome transaction as SubmitPayloadV1. */
XL6_CODE_LEASE_API struct xfuture* xl6TaskSubmitOwnedPolicyV1(XL6_TaskPool*,void*,
    struct xcancel*,const struct xfuturepayloadownershipv1*,bool languageOutcome);
/* Actual pending language-output owner; not an adapter for native queued jobs
 * or permission to collect an accepted task before it has run. */
struct xfutureproducerownershipv1;
XL6_CODE_LEASE_API const struct xfutureproducerownershipv1* xl6TaskProducerPolicyV1Get(void);
struct xtaskdataownershipv1;
struct xfuturewatchownershipv1;
struct xrtownershipadapterv1;
struct xrtownershippreparationv1;
/* The native Job owns exactly the existing accepted frame reference. The
 * policy/Watch identities do not certify other frame layouts or their children.
 * V2 frame preparation waits for actual execution, cleanup and forwarding;
 * its real code lease stays held until the last physical frame reference. */
XL6_CODE_LEASE_API const struct xtaskdataownershipv1* xl6TaskNativeDataPolicyV1Get(void);
XL6_CODE_LEASE_API const struct xfuturewatchownershipv1* xl6TaskWatchPolicyV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6TaskOwnershipAdapterV1(
    struct xrtownershipref, const struct xrtownershippreparationv1**);
/* Borrowed view of a real owning node: Create owner, collector pins, accepted
 * frame references and native call/activity credits. Trace visits exactly the
 * native pool owner slot; active entries are not inspectable. Native pools,
 * Jobs and their Data policies require independent resolver certification.
 * Wrapper preparation closes new admission only. The complete graph must
 * prepare/join every native participant before Clear/Finish can release its
 * native owner. Existing callbacks perform all accepted work and cleanup.
 * PoolDestroy consumes Create's owner after quiescence; actual pins retain
 * the terminal shell and its synchronization resources through their tails. */
XL6_CODE_LEASE_API struct xrtownershipref xl6TaskPoolOwnership(const XL6_TaskPool*);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6TaskPoolOwnershipAdapterV1(
    struct xrtownershipref, const struct xrtownershippreparationv1**);
struct xrtownershipquiescencev1;
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6TaskPoolQuiescenceV1Get(void);
#endif
