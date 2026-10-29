/* Fixed-certificate managed Dial, literal loopback only. No DNS/probing. */
#define main tlsExecutionBaselineMain
#include "tls_execution_resource.c"
#undef main
#include "src/runtime/xl6_execution_tls_dial.h"
#include "src/runtime/xl6_execution_net_resolver.h"
#include "src/runtime/xl6_tls_dial_options_internal.h"
#include "src/runtime/xl6_tls_data_abi.h"
static Resource* resolver(Scene* s)
{
    xnetresolverconfig config;xrtNetResolverConfigInit(&config);
    Resource* r=xl6ExecutionNetResolverCreateV1(s->source,&config);CHECK(r);return r;
}
static void config(xtlsclientconfig* native,void* raw)
{
    XL6_TlsClientContextV1* c=raw;xrtTlsClientConfigInit(native);
    native->Context=c->parameters;native->Verifier=c->verifier.value;
    native->ServerName=(xstrview){"localhost",9};
}
static void options(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* base=xl6TlsDialOptionsCreateV1();CHECK(base);
    xtlsdialconfig expected;xrtTlsDialConfigInit(&expected);
    const XL6_TlsDialOptionsV1* actual=base;
    CHECK(actual->native.Timeout==expected.Timeout&&actual->native.ServerNameFromHost==expected.ServerNameFromHost);
    CHECK(actual->native.Transport.Timeout==expected.Transport.Timeout&&actual->native.Transport.FallbackDelay==expected.Transport.FallbackDelay);
    CHECK(actual->native.Transport.Stream.ReadSize==expected.Transport.Stream.ReadSize&&actual->native.Stream.AsyncBytesLimit==expected.Stream.AsyncBytesLimit);
    void* named=xl6TlsDialOptionsWithServerNameV1(base,(xstrview){"localhost\0extra",15},9);CHECK(named);
    void* verify=xl6TlsDialOptionsWithVerifyNameV1(named,(xstrview){"localhost",9},100);CHECK(verify);
    xl6TlsDialOptionsDestroyV1(base);xl6TlsDialOptionsDestroyV1(named);
    xstrview name=xl6TlsDialOptionsServerNameV1(verify);CHECK(name.Size==9&&!memcmp(name.Data,"localhost",9));xrtFree((void*)name.Data);
    name=xl6TlsDialOptionsVerifyNameV1(verify);CHECK(name.Size==9&&!memcmp(name.Data,"localhost",9));xrtFree((void*)name.Data);
    CHECK(!xl6TlsDialOptionsWithTimeoutV1(verify,0));xrtClearError();
    CHECK(!xl6TlsDialOptionsWithServerNameV1(verify,(xstrview){"a\0b",3},-1));xrtClearError();
    void* timed=xl6TlsDialOptionsWithTimeoutV1(verify,123);CHECK(timed&&xl6TlsDialOptionsTimeoutV1(timed)==123);
    xl6TlsDialOptionsDestroyV1(timed);xl6TlsDialOptionsDestroyV1(verify);balanced(before);
}
static void allocation(xnetportbackend backend)
{
    /* These are caller-thread budgets only. Worker-side crypto allocation is
     * intentionally outside this ordinary resource-adapter acceptance. */
    for(unsigned mode=0;mode<2;mode++){
        unsigned clean=0,failed=0;
        for(unsigned budget=0;budget<32&&clean<2;budget++){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
            Resource* names=resolver(&s);void* base=xl6TlsDialOptionsCreateV1();CHECK(base);
            CHECK(xrtMemDebugFailAfter(budget));
            void* result=mode?(void*)xl6ExecutionTlsPrepareDialV1(s.source,s.engine,names):
                xl6TlsDialOptionsWithServerNameV1(base,(xstrview){"localhost",9},-1);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            if(!result){CHECK(hit&&xrtGetError());failed++;clean=0;xrtClearError();}
            else{CHECK(!hit);clean++;if(mode)xl6ExecutionResourceReleaseV1(result);else xl6TlsDialOptionsDestroyV1(result);}
            xl6TlsDialOptionsDestroyV1(base);xl6ExecutionResourceReleaseV1(names);ordinaryDrop(&s);balanced(before);
        }CHECK(clean==2&&failed);
    }
}
static void dial(xnetportbackend backend,bool delivered,unsigned order)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
    Resource* names=resolver(&s);Resource* carrier=xl6ExecutionTlsPrepareDialV1(s.source,s.engine,names);CHECK(carrier);
    xl6ExecutionResourceReleaseV1(names);
    XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsListenerEnterV1(s.listener,true,&scope));
    xnetaddr address;CHECK(xrtTlsListenerLocal(scope.listener,&address));
    xfuture* accepted=delivered?xrtTlsListenerAcceptAsync(scope.listener):NULL;xl6ExecutionTlsLeaveV1(&scope);
    void *client=NULL,*server=NULL;contexts(&client,&server);xtlsclientconfig clientConfig;config(&clientConfig,client);
    xfuture* source=xl6ExecutionTlsDialSourceV1(carrier,"127.0.0.1",address.Port,&clientConfig,NULL);CHECK(source);
    xl6TlsClientContextDestroyV1(client);xl6TlsServerContextDestroyV1(server);
    CHECK(!xl6ExecutionTlsDialSourceV1(carrier,"127.0.0.1",address.Port,&clientConfig,NULL)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xl6ExecutionResourceReleaseV1(s.client);s.client=carrier;s.pending=source;
    if(delivered){
        CHECK(xrtFutureWaitFor(source,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(source)==XFUTURE_RESOLVED);
        CHECK(xrtFutureWaitFor(accepted,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(accepted)==XFUTURE_RESOLVED);
        Resource* peer=xl6ExecutionTlsAdoptV1(s.source,s.engine,xrtFutureValue(accepted));CHECK(peer);
        xrtFutureDestroy(accepted);xl6ExecutionResourceReleaseV1(s.peer);s.peer=peer;
        CHECK(xl6ExecutionResourceCloseV1(s.engine));
        CHECK(xl6ExecutionTlsPromoteDialV1(carrier));xrtFutureDestroy(source);s.pending=NULL;
        CHECK(!xl6ExecutionTlsPromoteDialV1(carrier));xrtClearError();
        xvalue* protocol=xl6TlsStreamProtocolV1(carrier,5000);CHECK(protocol);xrtValueRelease(protocol);
        CHECK(xl6TlsStreamSendTextV1(carrier,(xstrview){"x\0y",3},-1,5000)==3);
        xvalue* output=xl6TlsStreamRecvV1(peer,3,5000);xbytesview view;
        CHECK(output&&xrtValueGetBytes(output,&view)&&view.Size==3&&!memcmp(view.Data,"x\0y",3));xrtValueRelease(output);
        ordinaryDrop(&s);
    }else collect(&s,order);
    balanced(before);
}
#include "tls_managed_proxy_peer.h"
static void proxyDial(xnetportbackend backend,unsigned order)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
    TlsProxyPeer peer={0};xnetproxy* proxy=proxyStart(&peer,backend);
    Resource* names=resolver(&s);Resource* carrier=xl6ExecutionTlsPrepareDialV1(s.source,s.engine,names);CHECK(carrier);
    xl6ExecutionResourceReleaseV1(names);void *client=NULL,*server=NULL;contexts(&client,&server);
    xtlsclientconfig options;config(&options,client);
    xfuture* source=xl6ExecutionTlsProxyDialSourceV1(carrier,proxy,"127.0.0.1",443,&options,NULL);CHECK(source);
    xrtNetProxyRelease(proxy);xl6TlsClientContextDestroyV1(client);xl6TlsServerContextDestroyV1(server);
    CHECK(xrtFutureWaitFor(source,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(source)==XFUTURE_RESOLVED);peerWait(&peer.accepted);
    xl6ExecutionResourceReleaseV1(s.client);s.client=carrier;s.pending=source;
    collect(&s,order);proxyStop(&peer);balanced(before);
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));
    if(argc>1&&!strcmp(argv[1],"--serve-proxy")){
        TlsProxyPeer peer={0};xnetproxy* proxy=proxyStart(&peer,XNET_PORT_SELECT);
        xnetproxyinfo config;CHECK(xrtNetProxyInfo(proxy,&config));
        printf("%u\n",(unsigned)config.Port);fflush(stdout);
        CHECK(getchar()=='\n');peerWait(&peer.accepted);proxyStop(&peer);
        xrtNetProxyRelease(proxy);CHECK(xrtMemDebugReset());return 0;
    }
    const XL6_OwnershipDomainV1* domains[]={
        #include "tls-resource-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
#ifdef _WIN32
        backend=XNET_PORT_IOCP;
#else
        backend=XNET_PORT_EPOLL;
#endif
    }
    options();allocation(backend);dial(backend,true,0);dial(backend,false,0);dial(backend,false,1);proxyDial(backend,0);proxyDial(backend,1);
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    puts("TLS managed dial: owned Resolver/Future, single submit, promotion after Engine Close, child Worker IO, normal CONNECT/TLS, four graphs; balanced");return 0;
}
