#ifndef XL6_TASK_FRAME_INTERNAL_H
#define XL6_TASK_FRAME_INTERNAL_H
#include "xl6_task.h"
struct xrtownershipops;
extern const struct xrtownershipops xl6TaskFrameOwnershipOpsInternalV1;
/* Permanent resident pool capability, never module/user callback code. This
 * typed optional owner is the ONLY pool dependency retained by a shared frame.
 * Engine submissions leave both pool and binding NULL, so they cannot create
 * or link a computation pool as an accidental side effect. */
typedef struct XL6_TaskFramePoolV1 {
    bool (*Hold)(const void*);
    void (*Drop)(const void*);
    struct xrtownershipref (*Ownership)(const XL6_TaskPool*);
    void (*ActivityEnter)(XL6_TaskNativeScopeV1*,XL6_TaskPool*);
    void (*ActivityLeave)(XL6_TaskNativeScopeV1*);
} XL6_TaskFramePoolV1;
/* Borrowed synchronous submission transaction. Context is caller stack state,
 * never retained in the frame. Begin/End and Submit are resident typed-provider
 * code, not an arbitrary generated executor policy. Begin failure rolls itself
 * back; after a successful Begin exactly one End runs. All fallible output
 * preparation precedes Begin; failure consumes no caller frame or captures. */
typedef struct XL6_TaskFrameTargetV1 {
    void* context;
    bool (*Begin)(void*);
    void (*End)(void*);
    struct xfuture* (*Submit)(void*,void*,struct xcancel*,
        XL6_FuturePayloadTrace,const struct xfuturepayloadownershipv1*);
    XL6_TaskPool* pool;
    const XL6_TaskFramePoolV1* poolBinding;
} XL6_TaskFrameTargetV1;
void* xl6TaskFrameAllocateInternalV1(const XL6_CodeOwner*,size_t,XL6_TaskRun,XL6_TaskDrop,XL6_CodeContextTrace);
/* Call-only borrow from this actual active frame, never a hidden result pin.
 * Native result factories must clone it into the independently owned result. */
XL6_CodeLease* xl6TaskFrameCodeLeaseInternalV1(const void*);
void xl6TaskFrameDiscardInternalV1(void*);
struct xrtownershipref xl6TaskFrameOwnershipInternalV1(const void*);
struct xfuture* xl6TaskFrameSubmitInternalV1(void*,struct xcancel*,XL6_FuturePayloadTrace,
    const struct xfuturepayloadownershipv1*,bool,const XL6_TaskFrameTargetV1*);
#endif
