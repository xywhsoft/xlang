#include "xl6_net_engine_config_internal.h"
#include "xl6_execution_net_engine.h"

void* xl6NetEngineCreateDefaultV1(XL6_ExecutionServicesV1 services)
{void* result=xl6ExecutionNetEngineCreateV1(services.source,NULL);if(!result)xl6NetEngineFailedV1();return result;}
void* xl6NetEngineCreateWorkersV1(XL6_ExecutionServicesV1 services,int64_t workers)
{
    if(workers<0||(uint64_t)workers>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);xl6NetEngineFailedV1();return NULL;}
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=(uint32_t)workers;
    void* result=xl6ExecutionNetEngineCreateV1(services.source,&config);if(!result)xl6NetEngineFailedV1();return result;
}
void* xl6NetEngineCreateV1(XL6_ExecutionServicesV1 services,const void* raw)
{
    if(!raw){xrtSetErrorKind(XERR_ARGUMENT);xl6NetEngineFailedV1();return NULL;}
    const XL6_NetEngineConfigV1* config=raw;
    void* result=xl6ExecutionNetEngineCreateV1(services.source,&config->value);if(!result)xl6NetEngineFailedV1();return result;
}
void xl6NetEngineDestroyV1(void* engine){xl6ExecutionResourceReleaseV1(engine);}
void xl6NetEngineStartV1(void* engine){if(!xl6ExecutionNetEngineStartV1(engine))xl6NetEngineFailedV1();}
void xl6NetEngineStopV1(void* engine){if(!xl6ExecutionNetEngineStopV1(engine))xl6NetEngineFailedV1();}
bool xl6NetEngineTryStopV1(void* engine)
{bool ready=false;if(!xl6ExecutionNetEngineTryStopV1(engine,&ready))xl6NetEngineFailedV1();return ready;}
void xl6NetEngineCloseV1(void* engine){if(!xl6ExecutionResourceCloseV1(engine))xl6NetEngineFailedV1();}
bool xl6NetEngineWaitV1(void* engine,int64_t millis)
{bool ready=false;if(!xl6ExecutionNetEngineWaitV1(engine,millis,&ready))xl6NetEngineFailedV1();return ready;}
void xl6NetEnginePostV1(void* engine,XL6_Closure* work,uint64_t affinity)
{if(!xl6ExecutionNetEnginePostV1(engine,affinity,work))xl6NetEngineFailedV1();}
uint64_t xl6NetEngineAfterV1(void* engine,int64_t millis,XL6_Closure* work,uint64_t affinity)
{uint64_t id=0;if(!xl6ExecutionNetEngineAfterV1(engine,affinity,millis,work,&id))xl6NetEngineFailedV1();return id;}
void xl6NetEngineCancelTimerV1(void* engine,uint64_t id)
{if(!xl6ExecutionNetEngineCancelTimerV1(engine,id))xl6NetEngineFailedV1();}
void xl6NetEngineThrowV1(void* engine){if(!xl6ExecutionNetEngineCheckV1(engine))xl6NetEngineFailedV1();}
uint64_t xl6NetEngineFailureCountV1(void* engine)
{
    XL6_ExecutionFailureSnapshotV1 snapshot={.size=sizeof(snapshot)};
    if(!xl6ExecutionNetEngineFailuresV1(engine,&snapshot))xl6NetEngineFailedV1();
    return snapshot.count;
}
bool xl6NetEngineFailureSaturatedV1(void* engine)
{
    XL6_ExecutionFailureSnapshotV1 snapshot={.size=sizeof(snapshot)};
    if(!xl6ExecutionNetEngineFailuresV1(engine,&snapshot))xl6NetEngineFailedV1();
    return snapshot.saturated;
}
void* xl6NetEngineConfigurationV1(void* engine)
{
    XL6_NetEngineConfigV1 config;
    if(!xl6ExecutionNetEngineConfigV1(engine,&config.value,&config.buffers)){xl6NetEngineFailedV1();return NULL;}
    XL6_NetEngineConfigV1* result=xrtMalloc(sizeof(*result));
    if(result){*result=config;xl6NetEngineConfigRebindV1(result);}else xl6NetEngineFailedV1();return result;
}
static bool stats(void* engine,xnetenginestats* output)
{bool ok=xl6ExecutionNetEngineStatsV1(engine,output);if(!ok)xl6NetEngineFailedV1();return ok;}
int64_t xl6NetEngineStateV1(void* engine)
{xnetenginestats value;return stats(engine,&value)?value.State:0;}
uint64_t xl6NetEngineWorkersV1(void* engine)
{xnetenginestats value;return stats(engine,&value)?value.Workers:0;}
static bool backend(void* engine,int64_t worker,xnetportbackendinfo* info)
{
    if(worker<0||(uint64_t)worker>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);xl6NetEngineFailedV1();return false;}
    bool ok=xl6ExecutionNetEngineBackendV1(engine,(uint32_t)worker,info);if(!ok)xl6NetEngineFailedV1();return ok;
}
int64_t xl6NetEngineBackendV1(void* engine,int64_t worker)
{xnetportbackendinfo info;return backend(engine,worker,&info)?info.Backend:0;}
int64_t xl6NetEngineCapabilitiesV1(void* engine,int64_t worker)
{xnetportbackendinfo info;return backend(engine,worker,&info)?info.Capabilities:0;}
void* xl6NetEngineSnapshotV1(void* engine)
{
    xnetenginestats value;if(!stats(engine,&value))return NULL;
    xnetenginestats* result=xrtMalloc(sizeof(*result));if(result)*result=value;else xl6NetEngineFailedV1();return result;
}
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* resource=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&resource,&ops,&user)&&resource&&!user&&
        ops==xl6NetEngineHandlePolicyV1Get()->Ops&&visit(xl6ExecutionResourceOwnershipV1(resource),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetEngineHandlePolicyV1Get,xl6NetEngineDestroyV1,handleTrace)
