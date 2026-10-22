#include "net-file-execution-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_execution_net_engine.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
static unsigned budgets,failures,zeroCopies,sharedCloses;
static void waitFuture(xfuture* future)
{assert(future&&xrtFutureWaitFor(future,UINT64_C(10000000))==XWAIT_OK);}
static void idle(XL6_ContinuationExecutor* executor)
{double until=xrtTimer()+10;while(!xl6ContinuationExecutorIsIdle(executor)){assert(xrtTimer()<until);xrtThreadYield();}}
static void closeFile(XL6_RuntimeServicesV1 services,void* file)
{
    xfuture* close=xl6NetFileCloseV1(services,file);waitFuture(close);assert(xrtFutureState(close)==XFUTURE_RESOLVED);
    xrtFutureDestroy(close);xl6NetFileDestroyV1(file);idle(services.executor);
}
static void* openFile(XL6_ExecutionServicesV1 execution,void* engine)
{void* file=xl6NetFileCreateV1(execution,engine,XRT_STR_LITERAL("language-file.bin"),15,438,7,0);assert(file);return file;}
typedef struct Gate {xatomic32 entered,release;} Gate;
static void gateProc(xnetworker* worker,void* data)
{
    (void)worker;Gate* gate=data;xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&gate->release,XMEMORY_ACQUIRE))xrtThreadYield();
}
static void blockWorker(void* engine,Gate* gate)
{
    xrtAtomic32Init(&gate->entered,0);xrtAtomic32Init(&gate->release,0);
    XL6_ExecutionScopeV1 scope={0};assert(xl6ExecutionResourceEnterV1(engine,true,&scope));
    assert(xrtNetEnginePost(scope.engine,0,gateProc,gate));xl6ExecutionResourceLeaveV1(&scope);
    double until=xrtTimer()+10;while(!xrtAtomic32Load(&gate->entered,XMEMORY_ACQUIRE)){assert(xrtTimer()<until);xrtThreadYield();}
}
static void zeroCopyAndClose(XL6_ExecutionServicesV1 execution,XL6_RuntimeServicesV1 services,void* engine)
{
    void* file=openFile(execution,engine);size_t size=1024*1024;
    bytes allocation=xrtMalloc(size);assert(allocation);memset(allocation,'X',size);allocation[1]=0;
    xvalue* input=xrtValueBytesTake(&allocation,size);assert(input&&!allocation);
    Gate gate;blockWorker(engine,&gate);xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xfuture* writing=xl6NetFileWriteBytesV1(services,file,0,input,-1);assert(writing);
    xrtMemDebugSnapshot(&after);
    /* The Worker is deterministically blocked: a payload-sized native copy
     * cannot have completed/freed before this observation. */
    assert(after.LiveBytes>=before.LiveBytes&&after.LiveBytes-before.LiveBytes<size/2);
    assert(xl6NetFileBytesInputPolicyV1Get()->Ops==xrtValueOwnership(input).Ops);
    xrtValueRelease(input);xrtAtomic32Store(&gate.release,1,XMEMORY_RELEASE);waitFuture(writing);
    assert(xrtFutureState(writing)==XFUTURE_RESOLVED&&*(int64_t*)xrtFutureValue(writing)==(int64_t)size);
    xrtFutureDestroy(writing);idle(services.executor);++zeroCopies;
    xfuture* reading=xl6NetFileReadV1(services,file,0,(int64_t)size);waitFuture(reading);
    assert(xrtFutureState(reading)==XFUTURE_RESOLVED);xvalue* result=*(xvalue**)xrtFutureValue(reading);
    xbytesview view={0};assert(result&&xrtValueGetBytes(result,&view)&&view.Size==size&&view.Data[0]=='X'&&!view.Data[1]);
    xvalue* again=*(xvalue**)xrtFutureValue(reading);assert(again==result);xrtFutureDestroy(reading);idle(services.executor);
    blockWorker(engine,&gate);
    xfuture* first=xl6NetFileCloseV1(services,file);xfuture* second=xl6NetFileCloseV1(services,file);assert(first&&second);
    assert(xrtFutureCancel(first)&&xrtFutureState(second)==XFUTURE_PENDING);
    xrtAtomic32Store(&gate.release,1,XMEMORY_RELEASE);waitFuture(first);waitFuture(second);
    assert(xrtFutureState(first)==XFUTURE_CANCELLED&&xrtFutureState(second)==XFUTURE_RESOLVED);
    xrtFutureDestroy(first);xrtFutureDestroy(second);xl6NetFileDestroyV1(file);idle(services.executor);++sharedCloses;
}
static void allocationPrefixes(XL6_ExecutionServicesV1 execution,XL6_RuntimeServicesV1 services,void* engine)
{
    xvalue* bytes=xrtValueBytes(XRT_BYTES_LITERAL("a\0b"));assert(bytes);
    xbuffer* buffer=xrtBufferCreate();assert(buffer&&xrtBufferAppend(buffer,XRT_BYTES_LITERAL("a\0b")));
    for(unsigned kind=0;kind<5;++kind)for(unsigned budget=0;budget<24;++budget){
        void* file=kind?openFile(execution,engine):NULL;xfuture* future=NULL;
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(budget));
        switch(kind){
            case 0:file=xl6NetFileCreateV1(execution,engine,XRT_STR_LITERAL("language-file.bin"),15,438,7,0);break;
            case 1:future=xl6NetFileReadV1(services,file,0,3);break;
            case 2:future=xl6NetFileWriteBytesV1(services,file,0,bytes,-1);break;
            case 3:future=xl6NetFileWriteTextV1(services,file,0,XRT_STR_LITERAL("a\0b"),-1);break;
            case 4:future=xl6NetFileWriteBufferV1(services,file,0,buffer,-1);break;
            default:abort();
        }
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);++budgets;
        if(kind?future!=NULL:file!=NULL)assert(outcome.ok);
        else{assert(!outcome.ok&&triggered);++failures;}
        xrtErrorFree(outcome.error);xrtClearError();
        if(future){waitFuture(future);assert(xrtFutureState(future)!=XFUTURE_PENDING);xrtFutureDestroy(future);}
        if(file)closeFile(services,file);
        idle(services.executor);
    }
    xrtValueRelease(bytes);xrtBufferDestroy(buffer);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    /* Check length metadata before inspecting path content, even at this
     * resident C boundary. No file/engine/source is touched on refusal. */
    xl6_error_scope rejected={0};assert(xl6ErrorScopeBegin(&rejected));
    assert(!xl6NetFileCreateV1((XL6_ExecutionServicesV1){0},NULL,(xstrview){"x",SIZE_MAX},15,438,7,0));
    xl6_error_result error=xl6ErrorScopeEnd(&rejected);
    assert(!error.ok&&xrtErrorKind(error.error)==XERR_RANGE);xrtErrorFree(error.error);xrtClearError();
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainNetFileLanguageV1Get(),xl6OwnershipDomainNetFileBytesV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
    XL6_ExecutionServicesV1 execution={xl6ExecutionSourceCreateV1()};assert(execution.source);
    XL6_RuntimeServicesV1 services={xl6ContinuationExecutorCreateFixedV1(16,1),NULL};assert(services.executor);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.NodeCacheBytes=0;
#if defined(_WIN32)
    config.Backend=XNET_PORT_IOCP;
#else
    config.Backend=XNET_PORT_URING;
#endif
    void* engine=xl6ExecutionNetEngineCreateV1(execution.source,&config);assert(engine&&xl6ExecutionNetEngineStartV1(engine));
    zeroCopyAndClose(execution,services,engine);allocationPrefixes(execution,services,engine);
    xl6ExecutionResourceReleaseV1(engine);assert(xl6ExecutionSourceCloseV1(execution.source));double until=xrtTimer()+10;
    while(execution.source){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&execution.source);assert(result.ok);if(result.ready)break;assert(xrtTimer()<until);xrtThreadYield();}
    idle(services.executor);xl6ContinuationExecutorClose(services.executor);assert(xl6ContinuationExecutorDestroy(services.executor));
    xl6OwnershipCatalogDestroyV1(catalog);assert(remove("language-file.bin")==0);xrtClearError();assert(xrtMemDebugReset());
    assert(budgets==120&&failures&&zeroCopies==1&&sharedCloses==1);
    printf("NetFile resident: %u allocation prefixes, %u immediate failures, %u gated zero-copy, %u shared close; exact memory\n",budgets,failures,zeroCopies,sharedCloses);return 0;
}
