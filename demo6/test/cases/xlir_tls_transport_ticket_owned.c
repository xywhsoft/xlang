/* Actual generated typed/erased exports, Worker-thread OOM and Close. Only
 * ordinary self-created literal-loopback communication, no external traffic. */
#include "include/xlc.h"
#include "src/runtime/xl6_tls_ticket_result.h"
#include "src/runtime/xl6_tls_data_abi.h"
#include "src/runtime/xl6_tls_transport_internal.h"
#include "src/runtime/xl6_array_value_read.h"
#include "src/runtime/xl6_net_engine_abi.h"
#include <assert.h>
#include <stdio.h>
typedef struct Gate {
    xatomic32 entered,release,done;
    xnetpost begin;
    size_t point;
    bool hit;
    xfuture* future;
} Gate;
static void wait_for(xatomic32* flag)
{
    xdeadline due=xrtDeadlineAfter(10000000);
    while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(due));xrtThreadYield();}
}
static void block(xnetworker* worker,void* raw)
{
    (void)worker;Gate* gate=raw;assert(xrtMemDebugFailAfter(gate->point));
    xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);wait_for(&gate->release);
}
static void clear(xnetworker* worker,void* raw)
{
    (void)worker;Gate* gate=raw;gate->hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    assert(gate->future&&xrtFutureState(gate->future)!=XFUTURE_PENDING);
    xrtAtomic32Store(&gate->done,1,XMEMORY_RELEASE);
}
static void* native(xvalue* object,const XL6_NativeHandlePolicyV1* policy)
{
    xvalue* field=xrtValueObjectGet(object,(xstrview){"handle",6});void *raw=NULL,*user=NULL;const xvaluehandleops* ops=NULL;
    assert(field&&xrtValueGetHandle(field,&raw,&ops,&user)&&raw&&!user&&ops==policy->Ops);return raw;
}
static xvalue* resolved(xfuture* future)
{
    assert(future&&xrtFutureWaitUntil(future,xrtDeadlineAfter(10000000))==XWAIT_OK);
    xfutureresult result={0};assert(xrtFutureResult(future,&result)&&result.State==XFUTURE_RESOLVED&&result.Value);
    xvalue* value=xrtValueRetain(*(xvalue**)result.Value);assert(value);xrtFutureDestroy(future);return value;
}
static xfuture* pop(void* function,xvalue* client,bool dynamic)
{
    xl6_foreign_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));xfuture* result=NULL;
    if(dynamic){
        xvalue* boxed=((xvalue*(*)(xvalue*))function)(client);assert(boxed);
        result=xrtFutureRef(xrtValueGetFuture(boxed));xrtValueRelease(boxed);
    }else result=((xfuture*(*)(xvalue*))function)(client);
    xl6_foreign_error_result status=xl6ForeignCallEnd(&scope);assert(status.ok&&result);xrtErrorFree(status.error);return result;
}
static void barrier(xnetworker* worker)
{
    Gate gate={.point=SIZE_MAX};assert(xrtNetPostInit(&gate.begin));
    assert(xrtNetPost(worker,&gate.begin,block,&gate));wait_for(&gate.entered);
    /* A private already-resolved future supplies the common queue barrier. */
    xpromise* promise=xrtPromiseCreate(&gate.future,NULL);assert(promise&&xrtPromiseResolve(promise,NULL));
    /* TaskNet uses the public command FIFO; an internal post has a separate
     * priority queue and cannot serve as its completion/fault-clear barrier. */
    assert(xrtNetEnginePost(xrtNetWorkerEngine(worker),xrtNetWorkerIndex(worker),clear,&gate));
    xrtAtomic32Store(&gate.release,1,XMEMORY_RELEASE);wait_for(&gate.done);
    xrtPromiseDestroy(promise);xrtFutureDestroy(gate.future);
}
int main(int argc,char** argv)
{
    assert(argc==6&&xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=".";options.tccDllPath=argv[3];options.tccLibPath=argv[4];options.tempDir=argv[5];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler&&xlcAddIncludePath(compiler,argv[2])&&xlcAddSourceFile(compiler,argv[1]));
    XL_Module* module=xlcRelocateModule(compiler);if(!module)fprintf(stderr,"%s\n",xlcGetLastError(compiler));
    /* Exported asynchronous calls need the persistent activation contract;
     * RunModuleMain is deliberately one-shot and retires language services. */
    assert(module&&xlActivateModule(xlcRuntimeContext(compiler),module,NULL));
    xfuture*(*create)(int64_t)=(xfuture*(*)(int64_t))xlModuleGetSymbol(module,"xl6_export_tls_transport_ticket_fixture");
    xfuture*(*seed)(xvalue*)=(xfuture*(*)(xvalue*))xlModuleGetSymbol(module,"xl6_export_tls_transport_ticket_seed");
    void* functions[]={xlModuleGetSymbol(module,"xl6_export_tls_transport_ticket_pop"),xlModuleGetSymbol(module,"xl6_export_tls_transport_ticket_pop_dynamic")};
    assert(create&&seed&&functions[0]&&functions[1]);xvalue* state=resolved(create(1));
    xvalue* values[5];for(size_t i=0;i<5;++i){values[i]=xl6ArrayValueReadCopyV1(state,(int64_t)i);assert(values[i]);}
    void* client=native(values[0],xl6TlsStreamHandlePolicyV1Get());void* peer=native(values[1],xl6TlsStreamHandlePolicyV1Get());
    void* engine=native(values[2],xl6NetEngineHandlePolicyV1Get());void* listener=native(values[3],xl6TlsListenerHandlePolicyV1Get());
    XL6_TlsScopeV1 scope={0};assert(xl6ExecutionTlsStreamEnterV1(client,false,&scope));
    xnetworker* worker=xrtNetStreamWorker(xrtTlsStreamTransport(scope.stream));assert(worker);xl6ExecutionTlsLeaveV1(&scope);
    for(size_t i=0;i<4;++i){xrtValueRelease(resolved(seed(state)));xrtValueRelease(resolved(pop(functions[i%2],values[0],i%2!=0)));}
    barrier(worker);size_t failed=0,passed=0;
    for(size_t mode=0;mode<2;++mode)for(size_t point=0;point<64;++point){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
        xrtValueRelease(resolved(seed(state)));assert(xl6TlsStreamTicketCountV1(client,10000)==1);
        Gate gate={.point=point};assert(xrtNetPostInit(&gate.begin));
        assert(xrtNetPost(worker,&gate.begin,block,&gate));wait_for(&gate.entered);
        gate.future=pop(functions[mode],values[0],mode!=0);
        assert(xrtNetEnginePost(xrtNetWorkerEngine(worker),xrtNetWorkerIndex(worker),clear,&gate));
        xrtAtomic32Store(&gate.release,1,XMEMORY_RELEASE);wait_for(&gate.done);
        xfutureresult result={0};assert(xrtFutureResult(gate.future,&result));
        if(result.State==XFUTURE_RESOLVED){
            ++passed;assert(!gate.hit&&result.Value&&xl6TlsStreamTicketCountV1(client,10000)==0);
            XL6_TlsTicketResultV1* ticket=native(*(xvalue**)result.Value,xl6TlsTicketResultHandlePolicyV1Get());
            assert(ticket->status==XTLS_OK&&ticket->resume);
        }else{
            ++failed;assert(gate.hit&&result.State==XFUTURE_FAILED&&xl6TlsStreamTicketCountV1(client,10000)==1);
        }
        xrtFutureDestroy(gate.future);xrtClearError();
        if(xl6TlsStreamTicketCountV1(client,10000))xrtValueRelease(resolved(pop(functions[0],values[0],false)));
        barrier(worker);xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
        assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
        assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&!xrtGetError());
    }
    assert(failed&&passed);xrtValueRelease(resolved(seed(state)));
    Gate close={.point=SIZE_MAX};assert(xrtNetPostInit(&close.begin));
    assert(xrtNetPost(worker,&close.begin,block,&close));wait_for(&close.entered);
    close.future=pop(functions[0],values[0],false);
    xlDestroyModule(module);xlcDestroyContext(compiler);assert(!xlRuntimeDrainRetiringChecked(0).ready);
    assert(xrtNetEnginePost(xrtNetWorkerEngine(worker),xrtNetWorkerIndex(worker),clear,&close));
    xrtAtomic32Store(&close.release,1,XMEMORY_RELEASE);wait_for(&close.done);
    xvalue* result=resolved(close.future);XL6_TlsTicketResultV1* ticket=native(result,xl6TlsTicketResultHandlePolicyV1Get());assert(ticket->resume);
    xl6TlsStreamCloseV1(client);xl6TlsStreamCloseV1(peer);xl6TlsListenerCloseV1(listener);xl6NetEngineCloseV1(engine);assert(xl6NetEngineWaitV1(engine,10000));
    for(size_t i=0;i<5;++i)xrtValueRelease(values[i]);
    xrtValueRelease(state);
    assert(!xlRuntimeDrainRetiringChecked(0).ready);assert(xl6TlsTicketResultStatusV1(ticket)==XTLS_OK);xrtValueRelease(result);
    XL_RuntimeDrainResult retired=xlRuntimeDrainRetiringChecked(-1);assert(retired.ready&&retired.ok&&!xrtGetError());
    printf("TLS transport ticket ownership: 128 Worker OOM prefixes, %zu failures/%zu successes, typed/dynamic results, no lost ticket, balanced memory and result constructed after Close: ok\n",failed,passed);return 0;
}
