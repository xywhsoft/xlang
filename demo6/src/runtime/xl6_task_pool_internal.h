#ifndef XL6_TASK_POOL_INTERNAL_H
#define XL6_TASK_POOL_INTERNAL_H
#include "xl6_task.h"
/* Same-source resident boundary, not a generated/host SDK. The control pool
 * owns its implementation; worker frames borrow it only through real entry,
 * activity and accepted-frame references. No private field layout is shared. */
#if defined(__GNUC__) && !defined(_WIN32)
#define XL6_TASK_POOL_INTERNAL __attribute__((visibility("hidden")))
#else
#define XL6_TASK_POOL_INTERNAL
#endif
typedef XL6_TaskNativeScopeV1 TaskActivity;
XL6_TASK_POOL_INTERNAL XL6_TaskPool* xl6TaskPoolCreateInternalV1(const XL6_TaskPoolConfig*);
XL6_TASK_POOL_INTERNAL bool xl6TaskPoolQuiesceInternalV1(XL6_TaskPool*);
XL6_TASK_POOL_INTERNAL bool xl6TaskPoolDestroyInternalV1(XL6_TaskPool*);
XL6_TASK_POOL_INTERNAL bool xl6TaskPoolIsQuiescentInternalV1(const XL6_TaskPool*);
XL6_TASK_POOL_INTERNAL bool xl6TaskPoolIsClosingInternalV1(const XL6_TaskPool*);
XL6_TASK_POOL_INTERNAL bool xl6TaskPoolHoldInternalV1(const void*);
XL6_TASK_POOL_INTERNAL void xl6TaskPoolDropInternalV1(const void*);
XL6_TASK_POOL_INTERNAL void xl6TaskPoolEnterInternalV1(XL6_TaskPool*);
XL6_TASK_POOL_INTERNAL void xl6TaskPoolLeaveInternalV1(XL6_TaskPool*);
XL6_TASK_POOL_INTERNAL void xl6TaskActivityEnterInternalV1(TaskActivity*,XL6_TaskPool*);
XL6_TASK_POOL_INTERNAL void xl6TaskActivityLeaveInternalV1(TaskActivity*);
XL6_TASK_POOL_INTERNAL struct xtaskpool* xl6TaskSubmitBeginInternalV1(XL6_TaskPool*);
XL6_TASK_POOL_INTERNAL void xl6TaskSubmitEndInternalV1(XL6_TaskPool*);
#endif
