#define main udpBaselineRuntimeMain
#include "release/include/xl_clock.h"
#include "udp_language_runtime.c"
#undef main
#include "udp_transfer_test.h"
#include "src/runtime/xl6_net_batch_progress_internal.h"
static unsigned descriptorPrefixes,descriptorFailures,controlPrefixes,controlFailures;
static void awaitControl(xfuture* future)
{
    if(!future)fprintf(stderr,"control dispatch failed: %d %s\n",(int)xrtErrorKind(xrtGetError()),xrtErrorMessage(xrtGetError()));
    CHECK(future&&xrtFutureWait(future)==XWAIT_OK);xfutureresult r={0};CHECK(xrtFutureResult(future,&r));
    CHECK(xl6PayloadObserveOutcome(r.State,r.Error)&&r.Value&&*(bool*)r.Value);xrtFutureDestroy(future);
}
static void offlineTransfers(void)
{
    void* bytes=xl6NetBytesFromTextV1((xstrview){"r\0",2},-1);CHECK(bytes);
    xvalue* object=testObject(bytes,xl6NetBytesHandlePolicyV1Get(),2),* objects[]={object,object};
    xvalue* refs=testArray(2,objects,2);xrtValueRelease(object);
    XL6_NativeClassArrayContextV1 context=testContext(2,xl6NetBytesHandlePolicyV1Get());
    void* vector=xl6UdpMessageRefsV1(context,refs,3);CHECK(vector);
    xbuffer buffer={0};CHECK(xrtBufferAppend(&buffer,(xbytesview){(const uint8_t*)"b\0x",3}));
    for(unsigned kind=0;kind<3;++kind)for(unsigned budget=0;;++budget){
        CHECK(budget<128);xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error)&&xrtMemDebugFailAfter(budget));
        void* raw=kind==0?xl6UdpMessageBufferV1(&buffer,99):kind==1?xl6UdpMessageRefsV1(context,refs,3):xl6UdpMessageNoControlV1(vector);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result result=xl6ErrorScopeEnd(&error);
        CHECK((raw!=NULL)==result.ok&&(!raw)==hit);xrtErrorFree(result.error);
        if(raw){CHECK(xl6UdpMessageLengthV1(raw)==3);xl6UdpMessageDestroyV1(raw);}else ++descriptorFailures;
        balanced(&before);++descriptorPrefixes;if(!hit)break;
    }
    xl6UdpMessageDestroyV1(vector);xrtValueRelease(refs);xrtBufferUnit(&buffer);
}
typedef struct WorkerCheck {void* receiver;xatomic32 done;bool foreign;} WorkerCheck;
static void onWorker(xnetworker* worker,void* raw)
{
    (void)worker;WorkerCheck* c=raw;xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error));
    xl6UdpMulticastLoopV1(c->receiver,true);
    if(c->foreign)endError(&error,XERR_STATE);else endOk(&error);
    xrtAtomic32Store(&c->done,1,XMEMORY_RELEASE);
}
typedef struct Gate {xatomic32 entered,release,done;} Gate;
static void blockWorker(xnetworker* worker,void* raw)
{
    (void)worker;Gate* g=raw;xrtAtomic32Store(&g->entered,1,XMEMORY_RELEASE);
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtAtomic32Load(&g->release,XMEMORY_ACQUIRE)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    xrtAtomic32Store(&g->done,1,XMEMORY_RELEASE);
}
static void waitFlag(xatomic32* flag)
{XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}}
static void blockedControls(Graph* graph,size_t index)
{
    Gate gate={0};CHECK(xrtNetEnginePost(graph->engine->engine,index,blockWorker,&gate));waitFlag(&gate.entered);
    xfuture* cancelled=xl6UdpMulticastLoopAsyncV1(graph->sender,true);CHECK(cancelled&&xrtFutureCancel(cancelled));
    xrtAtomic32Store(&gate.release,1,XMEMORY_RELEASE);waitFlag(&gate.done);CHECK(xrtFutureWait(cancelled)==XWAIT_OK);
    xfutureresult outcome={0};CHECK(xrtFutureResult(cancelled,&outcome)&&outcome.State==XFUTURE_CANCELLED);xrtFutureDestroy(cancelled);
    if(controlPrefixes)return;
    /* Keep the actual Worker busy so injected allocation failure only tests
     * submission/rollback, not unrelated asynchronous OS work. */
    for(unsigned budget=0;;++budget){
        CHECK(budget<128);gate=(Gate){0};CHECK(xrtNetEnginePost(graph->engine->engine,index,blockWorker,&gate));waitFlag(&gate.entered);
        xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error)&&xrtMemDebugFailAfter(budget));
        xfuture* future=xl6UdpMulticastLoopAsyncV1(graph->sender,true);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result result=xl6ErrorScopeEnd(&error);
        CHECK((future!=NULL)==result.ok&&(!future)==hit);xrtErrorFree(result.error);
        xrtAtomic32Store(&gate.release,1,XMEMORY_RELEASE);waitFlag(&gate.done);
        if(future)awaitControl(future);else ++controlFailures;
        ++controlPrefixes;if(!hit)break;
    }
}
static void transfer(Graph* graph)
{
    xvalue* texts=testTexts(2);CHECK(xl6UdpSendVectorTextV1(graph->sender,texts,5)==XNET_RESULT_OK);receive(graph,"A\0B\xe4\xbd",5);xrtValueRelease(texts);
    texts=testTexts(0);CHECK(xl6UdpSendVectorTextV1(graph->sender,texts,-1)==XNET_RESULT_OK);receive(graph,NULL,0);xrtValueRelease(texts);
    xvalue* bytes=testBytes();CHECK(xl6UdpSendVectorBytesV1(graph->sender,bytes,99)==XNET_RESULT_OK);receive(graph,"b\0b\0",4);xrtValueRelease(bytes);
    void* buf=xl6BufferCreateV1();CHECK(buf&&xrtBufferAppend(buf,(xbytesview){(const uint8_t*)"q\0",2}));
    xvalue* object=testObject(buf,xl6BufferHandlePolicyV1Get(),1),* objects[]={object,object};xvalue* buffers=testArray(1,objects,2);xrtValueRelease(object);
    XL6_NativeClassArrayContextV1 bc=testContext(1,xl6BufferHandlePolicyV1Get());
    CHECK(xl6UdpSendVectorBufferV1(bc,graph->sender,buffers,3)==XNET_RESULT_OK);receive(graph,"q\0q",3);
    xnetdgramcontrol control={0};CHECK(xl6UdpSendVectorConnectedMessageBufferV1(bc,graph->sender,&control,buffers,99)==XNET_RESULT_OK);receive(graph,"q\0q\0",4);
    void* copied=xl6UdpMessageBufferV1(buf,-1);CHECK(copied);((xbuffer*)buf)->Data[0]='x';
    CHECK(xl6UdpSendMessageV1(graph->sender,copied)==XNET_RESULT_OK);receive(graph,"q\0",2);xl6UdpMessageDestroyV1(copied);xrtValueRelease(buffers);
    void* raw=xl6NetBytesFromTextV1((xstrview){"r\0",2},-1);CHECK(raw);
    object=testObject(raw,xl6NetBytesHandlePolicyV1Get(),2);objects[0]=object;objects[1]=object;
    xvalue* refs=testArray(2,objects,2);xrtValueRelease(object);XL6_NativeClassArrayContextV1 nc=testContext(2,xl6NetBytesHandlePolicyV1Get());
    CHECK(xl6UdpSendRefsV1(nc,graph->sender,refs,3)==XNET_RESULT_OK);receive(graph,"r\0r",3);
    void* vector=xl6UdpMessageRefsV1(nc,refs,3);CHECK(vector);xrtValueRelease(refs);
    void* address=xl6UdpLocalV1(graph->receiver);CHECK(address);
    xnetudppacket* packet=udpFixturePacket(address);CHECK(packet);
    void* p=xl6UdpMessageRefPacketV1(packet,3,1);CHECK(p);xrtNetUdpPacketDestroy(packet);
    void* text=xl6UdpMessageTextV1((xstrview){"m\0",2},-1);CHECK(text);
    xvalue* messages[3]={testObject(text,xl6UdpMessageHandlePolicyV1Get(),3),testObject(p,xl6UdpMessageHandlePolicyV1Get(),3),testObject(vector,xl6UdpMessageHandlePolicyV1Get(),3)};
    xvalue* batch=testArray(3,messages,3);for(size_t i=0;i<3;++i)xrtValueRelease(messages[i]);
    void* progress=xl6NetBatchProgressCreateV1();CHECK(progress);
    XL6_NativeClassArrayContextV1 mc=testContext(3,xl6UdpMessageHandlePolicyV1Get());
    XL6_ProductI64U64V1 result=xl6UdpSendMessagesV1(mc,graph->sender,batch,progress,0,99);
    CHECK(result.e0==XNET_RESULT_OK&&result.e1==3&&xl6NetBatchProgressAttemptedV1(progress));
    receive(graph,"m\0",2);receive(graph,"p\0q",3);receive(graph,"r\0r",3);
    xnetudpconfig config;xrtNetUdpConfigInit(&config);config.SendPacketLimit=1;
    void* limited=xl6UdpConnectV1((XL6_ExecutionServicesV1){graph->source},graph->engine,address,&config,UINT64_MAX);CHECK(limited);
    own(graph,limited,xl6UdpHandlePolicyV1Get());CHECK(xl6UdpWaitV1(limited,XNET_UDP_WAIT_OPEN,5000)==XNET_RESULT_OK);
    result=xl6UdpSendMessagesV1(mc,limited,batch,progress,0,2);
    CHECK(result.e0==XNET_RESULT_AGAIN&&result.e1==1&&xl6NetBatchProgressCountV1(progress)==1);receive(graph,"m\0",2);
    CHECK(xl6UdpWaitWritableV1(limited,0,5000)==XNET_RESULT_OK);
    result=xl6UdpSendMessagesV1(mc,limited,batch,progress,1,1);CHECK(result.e0==XNET_RESULT_OK&&result.e1==1);receive(graph,"p\0q",3);
    CHECK(xl6NetBatchProgressStartV1(progress)==1);
    object=testObject(xl6UdpMessageNoControlV1(text),xl6UdpMessageHandlePolicyV1Get(),3);
    xvalue* repeated[65];for(size_t i=0;i<65;++i)repeated[i]=object;
    xvalue* large=testArray(3,repeated,65);xrtValueRelease(object);
    CHECK(xl6UdpWaitWritableV1(limited,0,5000)==XNET_RESULT_OK);
    xl6_error_scope allocation={0};CHECK(xl6ErrorScopeBegin(&allocation)&&xrtMemDebugFailAfter(0));
    result=xl6UdpSendMessagesV1(mc,limited,large,progress,0,-1);
    CHECK(xrtMemDebugFailTriggered());xrtMemDebugFailClear();endError(&allocation,XERR_MEMORY);
    CHECK(result.e0==XNET_RESULT_ERROR&&!xl6NetBatchProgressAttemptedV1(progress));
    result=xl6UdpSendMessagesV1(mc,limited,large,progress,0,-1);
    CHECK(result.e0==XNET_RESULT_AGAIN&&result.e1==1);receive(graph,"m\0",2);
    CHECK(xl6UdpWaitWritableV1(limited,0,5000)==XNET_RESULT_OK);
    result=xl6UdpSendMessagesV1(mc,limited,large,progress,64,1);
    CHECK(result.e0==XNET_RESULT_OK&&result.e1==1&&xl6NetBatchProgressStartV1(progress)==64);receive(graph,"m\0",2);
    xrtValueRelease(large);
    xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error));result=xl6UdpSendMessagesV1(mc,graph->sender,batch,progress,-1,-1);CHECK(result.e0==XNET_RESULT_ERROR);
    endError(&error,XERR_RANGE);CHECK(!xl6NetBatchProgressAttemptedV1(progress));
    void* part=xl6NetDatagramPacketCreateTextV1((xstrview){"copy\0",5},-1);CHECK(part);
    object=testObject(part,xl6NetDatagramPacketHandlePolicyV1Get(),4);objects[0]=object;objects[1]=object;xvalue* packets=testArray(4,objects,2);xrtValueRelease(object);
    result=xl6UdpSendPacketBatchV1(testContext(4,xl6NetDatagramPacketHandlePolicyV1Get()),graph->sender,packets,progress,0,-1);
    CHECK(result.e0==XNET_RESULT_OK&&result.e1==2);receive(graph,"copy\0",5);receive(graph,"copy\0",5);xrtValueRelease(packets);
    xl6NetBatchProgressDestroyV1(progress);
    xvalue* tooMany=testTexts(65);CHECK(xl6ErrorScopeBegin(&error));CHECK(xl6UdpSendVectorTextV1(graph->sender,tooMany,0)==XNET_RESULT_ERROR);endError(&error,XERR_RANGE);xrtValueRelease(tooMany);
    xl6UdpMulticastLoopV1(graph->sender,true);xl6UdpMulticastHopLimitV1(graph->sender,1);
    xnetaddr iface;CHECK(xrtNetAddrLoopback(&iface,XNET_FAMILY_IPV4,0));xl6UdpMulticastInterfaceV1(graph->sender,&iface);
    awaitControl(xl6UdpMulticastLoopAsyncV1(graph->sender,false));awaitControl(xl6UdpMulticastHopLimitAsyncV1(graph->sender,2));
    awaitControl(xl6UdpMulticastInterfaceAsyncV1(graph->sender,&iface));awaitControl(xl6UdpMulticastInterfaceDefaultAsyncV1(graph->sender));
    XL6_UdpScopeV1 scope={0};CHECK(xl6ExecutionUdpEnterInternalV1(graph->sender,false,&scope));
    size_t index=xrtNetWorkerIndex(xrtNetUdpWorker(scope.udp));xl6ExecutionUdpLeaveInternalV1(&scope);
    WorkerCheck worker={.receiver=graph->sender};CHECK(xrtNetEnginePost(graph->engine->engine,index,onWorker,&worker));waitFlag(&worker.done);
    worker=(WorkerCheck){.receiver=graph->sender,.foreign=true};CHECK(xrtNetEnginePost(graph->engine->engine,1-index,onWorker,&worker));waitFlag(&worker.done);
    blockedControls(graph,index);
    CHECK(xl6ErrorScopeBegin(&error));CHECK(!xl6UdpMulticastHopLimitAsyncV1(graph->sender,256));endError(&error,XERR_RANGE);
    xl6NetAddressDestroyV1(address);
    CHECK(graph->rootsCount<ROOTS);graph->roots[graph->rootsCount++]=batch;
    /* Do not wait for the final owned command. Checked graph retirement must
     * account for a queued/running/done Command -> Promise -> Future chain,
     * not depend on callers having awaited every configuration operation. */
    xfuture* pending=xl6UdpMulticastLoopAsyncV1(graph->sender,true);CHECK(pending);
    xvalue* futureRoot=xrtValueFuture(pending);CHECK(futureRoot);xrtFutureDestroy(pending);
    CHECK(graph->rootsCount<ROOTS);graph->roots[graph->rootsCount++]=futureRoot;
}
int main(int argc,char** argv)
{
    CHECK(udpBaselineRuntimeMain(argc,argv)==0);
    const XL6_OwnershipDomainV1* domains[]={
        #include "udp-language-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));CHECK(catalog);
    offlineTransfers();
    xnetportbackend backend=XNET_PORT_SELECT;if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned order=0;order<8;++order){xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};
        create(&graph,backend);transfer(&graph);collect(&graph,order);balanced(&before);}
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("UDP transfer runtime: graphs=%u roundtrips=%u descriptor-prefixes=%u/%u control-prefixes=%u/%u; vectors, mixed/copy batches, prefix acceptance, main/Worker/cancel/OOM controls; balanced\n",graphs,roundtrips,descriptorPrefixes,descriptorFailures,controlPrefixes,controlFailures);return 0;
}
