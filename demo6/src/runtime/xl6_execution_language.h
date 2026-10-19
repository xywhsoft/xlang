#ifndef XL6_EXECUTION_LANGUAGE_H
#define XL6_EXECUTION_LANGUAGE_H
#include "xl6_code_lease.h"
#include "xl6_checked_result_closure.h"
struct XL6_ExecutionSourceV1;
struct xlir_closure;
struct xvalue;
struct XL6_NativeHandlePolicyV1;
/* Borrowed call-only service view, independent of the immutable continuation
 * and deadline service ABI. The module owns and traces the actual source. */
typedef struct XL6_ExecutionServicesV1 {struct XL6_ExecutionSourceV1* source;} XL6_ExecutionServicesV1;
XL6_CODE_LEASE_API void* xl6ExecutorCreateLanguageV1(XL6_ExecutionServicesV1,int64_t,int64_t,int64_t);
XL6_CODE_LEASE_API void xl6ExecutorDestroyLanguageV1(void*);
XL6_CODE_LEASE_API void xl6ExecutorSubmitLanguageV1(void*,struct xlir_closure*);
typedef struct XL6_NullaryVoidArrayContextV1 {
    uint64_t arrayIdentity,signatureIdentity;
} XL6_NullaryVoidArrayContextV1;
XL6_CODE_LEASE_API void xl6ExecutorSubmitBatchLanguageV1(XL6_NullaryVoidArrayContextV1,void*,const struct xvalue*);
XL6_CODE_LEASE_API void xl6ExecutorCloseLanguageV1(void*);
XL6_CODE_LEASE_API void xl6ExecutorCancelLanguageV1(void*);
XL6_CODE_LEASE_API bool xl6ExecutorWaitLanguageV1(void*,int64_t);
XL6_CODE_LEASE_API void xl6ExecutorThrowLanguageV1(void*);
XL6_CODE_LEASE_API uint64_t xl6ExecutorFailureCountLanguageV1(void*);
XL6_CODE_LEASE_API bool xl6ExecutorFailureSaturatedLanguageV1(void*);
XL6_CODE_LEASE_API void* xl6ExecutorSnapshotLanguageV1(void*);
XL6_CODE_LEASE_API void xl6ExecutorSnapshotDestroyLanguageV1(void*);
#define XL6_EXECUTOR_STAT_DECL(Name) XL6_CODE_LEASE_API uint64_t xl6ExecutorSnapshot##Name##LanguageV1(const void*);
XL6_EXECUTOR_STAT_DECL(Threads)
XL6_EXECUTOR_STAT_DECL(QueueLimit)
XL6_EXECUTOR_STAT_DECL(Queued)
XL6_EXECUTOR_STAT_DECL(Running)
XL6_EXECUTOR_STAT_DECL(Submitted)
XL6_EXECUTOR_STAT_DECL(Completed)
XL6_EXECUTOR_STAT_DECL(Executed)
XL6_EXECUTOR_STAT_DECL(Stolen)
XL6_EXECUTOR_STAT_DECL(Cancelled)
XL6_EXECUTOR_STAT_DECL(Rejected)
#undef XL6_EXECUTOR_STAT_DECL
XL6_CODE_LEASE_API bool xl6ExecutorSnapshotClosedLanguageV1(const void*);
XL6_CODE_LEASE_API bool xl6ExecutorSnapshotCancellingLanguageV1(const void*);
XL6_CODE_LEASE_API const struct XL6_NativeHandlePolicyV1* xl6ExecutorHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const struct XL6_NativeHandlePolicyV1* xl6ExecutorSnapshotHandlePolicyV1Get(void);
#endif
