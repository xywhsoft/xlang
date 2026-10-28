#ifndef XL6_EXECUTION_RESOURCE_H
#define XL6_EXECUTION_RESOURCE_H
#include "xl6_task.h"
#include "xl6_executor_jobs.h"
struct xexecutorstats;
struct XL6_ExecutionFamilyV1;
struct xnetengine;
struct xnetresolver;
typedef struct XL6_ExecutionSourceV1 XL6_ExecutionSourceV1;
typedef struct XL6_ExecutionResourceV1 XL6_ExecutionResourceV1;
typedef enum XL6_ExecutionKindV1 {
    XL6_EXECUTION_EXECUTOR_V1=1,
    XL6_EXECUTION_TASK_POOL_V1=2,
    XL6_EXECUTION_NET_ENGINE_V1=3,
    XL6_EXECUTION_NET_RESOLVER_V1=4,
    XL6_EXECUTION_TCP_STREAM_V1=5,
    XL6_EXECUTION_TCP_LISTENER_V1=6,
    XL6_EXECUTION_TCP_SERVER_V1=7,
    XL6_EXECUTION_UDP_SOCKET_V1=8,
    XL6_EXECUTION_NET_FILE_V1=9,
    XL6_EXECUTION_TLS_STREAM_V1=10,
    XL6_EXECUTION_TLS_LISTENER_V1=11,
    XL6_EXECUTION_WS_STREAM_V2=12
} XL6_ExecutionKindV1;
typedef struct XL6_ExecutionSourceSnapshotV1 {
    size_t size,resources,entries;
    bool closed,active,exited,failed;
} XL6_ExecutionSourceSnapshotV1;
typedef struct XL6_ExecutionResourceSnapshotV1 {
    size_t size,owners,entries;
    XL6_ExecutionKindV1 kind;
    bool closing,nativePresent,stalled;
} XL6_ExecutionResourceSnapshotV1;
typedef struct XL6_ExecutionRetireResultV1 { bool ready,ok; } XL6_ExecutionRetireResultV1;

/* Explicit owning source for execution resources, not a default scheduler or
 * a per-job helper. One native control thread only retires closed instances;
 * normal bodies still run on their XRT executor/pool. Close refuses NEW
 * resources, but does not cancel/close still-owned instances. Last user owner
 * or an unreachable instance's graph preparation requests their retirement.
 * Busy work is polled only while a closing instance is pending, never while
 * idle. A native cleanup failure retains the real resource and latches the
 * first failure (including NULL diagnostic); Retry is explicit, no blind
 * destructive retries. All native calls/joins run outside metadata locks and
 * ownership mutation. Source creator must survive until successful Destroy.
 * No code owner, language callback or arbitrary resource policy is retained. */
XL6_CODE_LEASE_API XL6_ExecutionSourceV1* xl6ExecutionSourceCreateV1(void);
XL6_CODE_LEASE_API bool xl6ExecutionSourceCloseV1(XL6_ExecutionSourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionSourceCheckV1(XL6_ExecutionSourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionSourceRetryV1(XL6_ExecutionSourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionSourceSnapshotV1Get(XL6_ExecutionSourceV1*,XL6_ExecutionSourceSnapshotV1*);
/* Serialized terminal creator; never waits on live work or its own control
 * callback. On ready, consumes and NULLs *source even if historical ok=false;
 * the caller receives the original diagnostic before physical destruction.
 * On BUSY/failed readiness the exact creator slot remains available. */
XL6_CODE_LEASE_API XL6_ExecutionRetireResultV1 xl6ExecutionSourceDestroyV1(XL6_ExecutionSourceV1**);

/* Native creation is transactional; service admission is rechecked before
 * publishing an instance. Exactly one returned user owner and one registered
 * source owner. Resource owns the source back-reference until unregistration.
 * Inspection holds are NOT user owners and cannot delay automatic close. */
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionExecutorCreateV1(XL6_ExecutionSourceV1*,uint32_t,size_t,size_t);
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTaskPoolCreateV1(XL6_ExecutionSourceV1*,uint32_t,size_t,size_t);
XL6_CODE_LEASE_API bool xl6ExecutionTaskPoolCheckExitV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionTaskPoolExitFailuresV1(XL6_ExecutionResourceV1*,XL6_ExecutionFailureSnapshotV1*);
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionResourceRetainV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API void xl6ExecutionResourceReleaseV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionResourceCloseV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionResourceRetryV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionResourceSnapshotV1Get(XL6_ExecutionResourceV1*,XL6_ExecutionResourceSnapshotV1*);

/* Physical native call view: zero-initialize, noncopyable, same call/thread.
 * Entry owns a real receiver reference until Leave and prevents concurrent
 * native destruction, including a callback releasing the last user owner.
 * Enter(false) permits reads/control after Close, Enter(true) requires open
 * admission. Returned pointers are BORROWED, never store past Leave. The
 * exact kind selects which typed pointer is populated; no void* guessing. */
typedef struct XL6_ExecutionScopeV1 {
    XL6_ExecutionResourceV1* resource;
    struct xexecutor* executor;
    XL6_TaskPool* pool;
    uint64_t thread;
    struct xnetengine* engine;
    struct xnetresolver* resolver;
    bool control;
} XL6_ExecutionScopeV1;
XL6_CODE_LEASE_API bool xl6ExecutionResourceEnterV1(XL6_ExecutionResourceV1*,bool,XL6_ExecutionScopeV1*);
/* Exclusive physical lifecycle operation; fails rather than waiting on another
 * entry (including a reentrant worker callback). No metadata lock or ownership
 * mutation remains held by the returned scope. Ordinary Enter cannot overlap. */
XL6_CODE_LEASE_API bool xl6ExecutionResourceControlV1(XL6_ExecutionResourceV1*,bool,XL6_ExecutionScopeV1*);
XL6_CODE_LEASE_API void xl6ExecutionResourceLeaveV1(XL6_ExecutionScopeV1*);
XL6_CODE_LEASE_API bool xl6ExecutionExecutorSubmitV1(XL6_ExecutionResourceV1*,XL6_Closure*);
XL6_CODE_LEASE_API bool xl6ExecutionExecutorSubmitBatchV1(XL6_ExecutionResourceV1*,XL6_Closure* const*,size_t);
XL6_CODE_LEASE_API bool xl6ExecutionExecutorWaitV1(XL6_ExecutionResourceV1*,int64_t,XL6_ExecutorWaitStatusV1*);
XL6_CODE_LEASE_API bool xl6ExecutionExecutorThrowIfFailedV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionExecutorFailuresV1(XL6_ExecutionResourceV1*,XL6_ExecutionFailureSnapshotV1*);
XL6_CODE_LEASE_API bool xl6ExecutionExecutorStatsV1(XL6_ExecutionResourceV1*,struct xexecutorstats*);
XL6_CODE_LEASE_API bool xl6ExecutionResourceCancelV1(XL6_ExecutionResourceV1*);

XL6_CODE_LEASE_API struct xrtownershipref xl6ExecutionSourceOwnershipV1(const XL6_ExecutionSourceV1*);
XL6_CODE_LEASE_API struct xrtownershipref xl6ExecutionResourceOwnershipV1(const XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ExecutionOwnershipAdapterV1(
    struct xrtownershipref,const struct XL6_ExecutionFamilyV1* const*,size_t,
    const struct xrtownershippreparationv1**);
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6ExecutionSourceQuiescenceV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6ExecutionResourceQuiescenceV1Get(void);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionExecutorFamilyV1Get(void);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionTaskPoolFamilyV1Get(void);
#endif
