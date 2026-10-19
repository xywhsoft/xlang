#ifndef XL6_EXECUTION_RESOURCE_INTERNAL_H
#define XL6_EXECUTION_RESOURCE_INTERNAL_H
#include "include/xruntime_decl.h"
#include "xl6_execution_resource.h"
/* Same resident build only. Concrete family tables and every transitive
 * callback are permanent runtime code, never generated/module-local policy.
 * A generic record is admitted by exact registered family identity BEFORE
 * graph Count/Trace dispatch. No void payload/function-pointer erasure.
 * Families are separate providers so Executor does not link TaskPool (or vice
 * versa); this declaration header has no native algorithm implementation. */
typedef struct XL6_ExecutionFamilyV1 {
    size_t size;
    uint64_t abiTag;
    XL6_ExecutionKindV1 kind;
    bool (*Create)(XL6_ExecutionResourceV1*,uint32_t,size_t,size_t);
    bool (*Rollback)(XL6_ExecutionResourceV1*);
    XL6_ModulePrepareResult (*Retire)(XL6_ExecutionResourceV1*);
    bool (*Finish)(XL6_ExecutionResourceV1*);
    void (*Drop)(XL6_ExecutionResourceV1*);
    bool (*Trace)(const XL6_ExecutionResourceV1*,xrtownershipvisitor,void*);
    bool (*Close)(XL6_ExecutionResourceV1*);
    bool (*Cancel)(XL6_ExecutionResourceV1*);
} XL6_ExecutionFamilyV1;
#define XL6_EXECUTION_FAMILY_TAG_V1 UINT64_C(0x584C364558464D31)
struct XL6_ExecutionResourceV1 {
    XL6_ExecutionSourceV1* source;
    const XL6_ExecutionFamilyV1* family;
    XL6_ExecutionResourceV1 *previous,*next;
    xexecutor* executor;
    XL6_TaskPool* pool;
    XL6_ExecutorFailuresV1* failures;
    xexecutorstats stats;
    size_t references,owners,entries;
    uint64_t cycle;
    XL6_ExecutionKindV1 kind;
    const void* claim;
    bool requested,retiring,stalled,cleared;
};
#if defined(__GNUC__) && !defined(_WIN32)
#define XL6_EXECUTION_INTERNAL __attribute__((visibility("hidden")))
#else
#define XL6_EXECUTION_INTERNAL
#endif
XL6_EXECUTION_INTERNAL XL6_ExecutionResourceV1* xl6ExecutionCreateInternalV1(
    XL6_ExecutionSourceV1*,const XL6_ExecutionFamilyV1*,uint32_t,size_t,size_t);
XL6_EXECUTION_INTERNAL bool xl6ExecutionRequestedInternalV1(const XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API const XL6_ExecutionFamilyV1* xl6ExecutionExecutorFamilyV1Get(void);
XL6_CODE_LEASE_API const XL6_ExecutionFamilyV1* xl6ExecutionTaskPoolFamilyV1Get(void);
#endif
