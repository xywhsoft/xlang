/* Reuse the actual static-role scene and physical graph driver. */
#define main tlsResourceBaselineMain
#include "tls_execution_resource.c"
#undef main
#include "src/runtime/xl6_tls_io_abi.h"
static void pendingAccept(xnetportbackend backend,unsigned order)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
    Resource* prepared=xl6ExecutionTlsPrepareAcceptV1(s.source,s.listener);CHECK(prepared);
    CHECK(!xl6ExecutionTlsPromoteAcceptV1(prepared)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    CHECK(!xl6ExecutionTlsAcceptSourceForV1(prepared,s.client)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xfuture* source=xl6ExecutionTlsAcceptSourceForV1(prepared,s.listener);CHECK(source&&xrtFutureState(source)==XFUTURE_PENDING);
    xfuture* same=xl6ExecutionTlsAcceptSourceForV1(prepared,s.listener);CHECK(same==source);xrtFutureDestroy(same);
    xl6ExecutionResourceReleaseV1(s.peer);s.peer=prepared;s.pending=source;collect(&s,order);balanced(before);
}
static void deliveredAccept(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
    Resource* prepared=xl6ExecutionTlsPrepareAcceptV1(s.source,s.listener);CHECK(prepared);
    xfuture* source=xl6ExecutionTlsAcceptSourceForV1(prepared,s.listener);CHECK(source);
    XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsListenerEnterV1(s.listener,false,&scope));
    xnetaddr remote;CHECK(xrtTlsListenerLocal(scope.listener,&remote));xl6ExecutionTlsLeaveV1(&scope);
    void *client=NULL,*server=NULL;contexts(&client,&server);
    Resource* connected=xl6TlsStreamCreateDefaultV1((XL6_ExecutionServicesV1){s.source},s.engine,&remote,client,(xstrview){"localhost",9},-1);CHECK(connected);
    xl6TlsClientContextDestroyV1(client);xl6TlsServerContextDestroyV1(server);
    CHECK(xrtFutureWaitFor(source,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(source)==XFUTURE_RESOLVED);
    CHECK(xl6ExecutionTlsPromoteAcceptV1(prepared));xrtFutureDestroy(source);
    xl6ExecutionResourceReleaseV1(s.client);xl6ExecutionResourceReleaseV1(s.peer);s.client=connected;s.peer=prepared;
    CHECK(xl6TlsStreamWaitV1(s.client,XTLS_STREAM_WAIT_OPEN,5000));
    CHECK(xl6TlsStreamSendTextV1(s.client,(xstrview){"a\0b",3},100,5000)==3);
    xvalue* received=xl6TlsStreamRecvV1(s.peer,3,5000);xbytesview view;
    CHECK(received&&xrtValueGetBytes(received,&view)&&view.Size==3&&!memcmp(view.Data,"a\0b",3));xrtValueRelease(received);
    CHECK(xl6TlsStreamSendTextV1(s.client,(xstrview){0},-1,5000)==0);
    ordinaryDrop(&s);balanced(before);
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));const XL6_OwnershipDomainV1* domains[]={
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
    pendingAccept(backend,0);pendingAccept(backend,1);deliveredAccept(backend);
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    puts("TLS IO resources: two pending accept graphs, exact receiver/single submission, real promotion, binary sync IO, empty send; balanced");return 0;
}
