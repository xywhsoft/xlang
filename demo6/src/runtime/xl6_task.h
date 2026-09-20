#ifndef XL6_TASK_H
#define XL6_TASK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "xl6_code_lease.h"
#include "xl6_future_payload.h"
#include "xl6_capture.h"
struct xcancel;
struct xfuture;
struct xrtownershipref;

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
/* Closed capture storage for verified callback activations. V1 allocation is
 * retained for other frame families until their own complete layout migration;
 * V2 does not certify the task pool, Watch or active worker as a graph leaf. */
typedef struct XL6_TaskRuntimeV2 {
    void* (*Allocate)(const XL6_CodeOwner*, size_t, XL6_TaskRun, const XL6_CaptureLayoutV1*);
    void (*Cleanup)(void*);
} XL6_TaskRuntimeV2;
XL6_CODE_LEASE_API const XL6_TaskRuntimeV2* xl6TaskRuntimeV2Get(void);
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
#endif
