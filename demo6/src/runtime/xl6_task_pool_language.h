#ifndef XL6_TASK_POOL_LANGUAGE_H
#define XL6_TASK_POOL_LANGUAGE_H
#include "xl6_execution_language.h"
#include "xl6_future_value.h"
XL6_CODE_LEASE_API void* xl6TaskPoolCreateLanguageV1(XL6_ExecutionServicesV1,int64_t,int64_t,int64_t);
XL6_CODE_LEASE_API void xl6TaskPoolDestroyLanguageV1(void*);
XL6_CODE_LEASE_API void xl6TaskPoolStartLanguageV1(void*);
XL6_CODE_LEASE_API struct xfuture* xl6TaskPoolSubmitLanguageV1(XL6_FutureValueProfileV1,void*,XL6_CheckedResultClosureBorrowV1);
XL6_CODE_LEASE_API struct xfuture* xl6TaskPoolSubmitCancelLanguageV1(XL6_FutureValueProfileV1,void*,XL6_CheckedResultClosureBorrowV1,void*);
XL6_CODE_LEASE_API void xl6TaskPoolCloseLanguageV1(void*);
XL6_CODE_LEASE_API void xl6TaskPoolCancelLanguageV1(void*);
XL6_CODE_LEASE_API bool xl6TaskPoolWaitLanguageV1(void*,int64_t);
XL6_CODE_LEASE_API bool xl6TaskPoolWaitUntilLanguageV1(void*,uint64_t);
XL6_CODE_LEASE_API bool xl6TaskPoolWaitCancelLanguageV1(void*,void*,int64_t);
XL6_CODE_LEASE_API bool xl6TaskPoolWaitUntilCancelLanguageV1(void*,uint64_t,void*);
XL6_CODE_LEASE_API void* xl6TaskPoolSnapshotLanguageV1(void*);
XL6_CODE_LEASE_API void xl6TaskPoolSnapshotDestroyLanguageV1(void*);
#define XL6_TASK_POOL_STAT_DECL(Name) XL6_CODE_LEASE_API uint64_t xl6TaskPoolSnapshot##Name##LanguageV1(const void*);
XL6_TASK_POOL_STAT_DECL(Threads)
XL6_TASK_POOL_STAT_DECL(QueueLimit)
XL6_TASK_POOL_STAT_DECL(Queued)
XL6_TASK_POOL_STAT_DECL(Running)
XL6_TASK_POOL_STAT_DECL(Submitted)
XL6_TASK_POOL_STAT_DECL(Completed)
XL6_TASK_POOL_STAT_DECL(Succeeded)
XL6_TASK_POOL_STAT_DECL(Failed)
XL6_TASK_POOL_STAT_DECL(Cancelled)
XL6_TASK_POOL_STAT_DECL(Rejected)
#undef XL6_TASK_POOL_STAT_DECL
XL6_CODE_LEASE_API bool xl6TaskPoolSnapshotInitializedLanguageV1(const void*);
XL6_CODE_LEASE_API bool xl6TaskPoolSnapshotClosedLanguageV1(const void*);
XL6_CODE_LEASE_API bool xl6TaskPoolSnapshotCancellingLanguageV1(const void*);
XL6_CODE_LEASE_API const struct XL6_NativeHandlePolicyV1* xl6TaskPoolHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const struct XL6_NativeHandlePolicyV1* xl6TaskPoolSnapshotHandlePolicyV1Get(void);
#endif
