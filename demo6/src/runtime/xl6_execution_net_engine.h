#ifndef XL6_EXECUTION_NET_ENGINE_H
#define XL6_EXECUTION_NET_ENGINE_H
#include "xl6_execution_resource.h"
struct xnetengineconfig;
struct xnetbufpoolconfig;
struct xnetportbackendinfo;
struct xnetenginestats;
struct xnettaskownershipv1;
struct xnettimerownershipv1;
struct xnetengineworkerexitownershipv1;
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionNetEngineCreateV1(
    XL6_ExecutionSourceV1*,const struct xnetengineconfig*);
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineStartV1(XL6_ExecutionResourceV1*);
/* Stop is restartable. Close is permanent language admission closure; Wait
 * requires Close and progresses a real nonwaiting native stop to its deadline.
 * A false checked result may still have ready=true after physical completion. */
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineStopV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineTryStopV1(XL6_ExecutionResourceV1*,bool*);
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineWaitV1(XL6_ExecutionResourceV1*,int64_t,bool*);
XL6_CODE_LEASE_API bool xl6ExecutionNetEnginePostV1(XL6_ExecutionResourceV1*,uint64_t,XL6_Closure*);
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineAfterV1(XL6_ExecutionResourceV1*,uint64_t,int64_t,XL6_Closure*,uint64_t*);
/* Successful cancellation only queues a request; it does not retract a body
 * which has already fired, or certify that captures have been dropped. */
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineCancelTimerV1(XL6_ExecutionResourceV1*,uint64_t);
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineCheckV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineFailuresV1(XL6_ExecutionResourceV1*,XL6_ExecutionFailureSnapshotV1*);
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineConfigV1(XL6_ExecutionResourceV1*,struct xnetengineconfig*,struct xnetbufpoolconfig*);
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineBackendV1(XL6_ExecutionResourceV1*,uint32_t,struct xnetportbackendinfo*);
XL6_CODE_LEASE_API bool xl6ExecutionNetEngineStatsV1(XL6_ExecutionResourceV1*,struct xnetenginestats*);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionNetEngineFamilyV1Get(void);
XL6_CODE_LEASE_API const struct xnettaskownershipv1* xl6NetEngineWorkPolicyV1Get(void);
XL6_CODE_LEASE_API const struct xnettimerownershipv1* xl6NetEngineTimerPolicyV1Get(void);
XL6_CODE_LEASE_API const struct xnetengineworkerexitownershipv1* xl6NetEngineExitPolicyV1Get(void);
#endif
