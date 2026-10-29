#include "release/include/xl_clock.h"
/* One valid local CONNECT tunnel endpoint followed by ordinary TLS. No
 * external target is contacted: 127.0.0.1:443 is only the fixed wire authority.
 * Test stack data survives all callbacks through explicit peer shutdown/join. */
typedef struct TlsProxyPeer {
    xnetengine* engine;xnetlistener* listener;void* role;
    xatomicptr tls;xatomic32 accepted,closed,listenerClosed;
} TlsProxyPeer;
static bool proxyAccept(xnetlistener* listener,xnetstream* stream,void* raw)
{(void)listener;return xrtNetStreamSetData(stream,raw);}
static void proxyClose(xtlsstream* stream,xnetresult result,const xerror* error,void* raw)
{(void)stream;(void)result;(void)error;xrtAtomic32Store(&((TlsProxyPeer*)raw)->closed,1,XMEMORY_RELEASE);}
static void proxyListenerClosed(xnetlistener* listener,void* raw)
{(void)listener;xrtAtomic32Store(&((TlsProxyPeer*)raw)->listenerClosed,1,XMEMORY_RELEASE);}
static void proxyRead(xnetstream* stream,xnetbuf* buffer,void* raw)
{
    TlsProxyPeer* peer=raw;char request[2048];size_t n=xrtNetBufSize(buffer);CHECK(n<sizeof(request));
    CHECK(xrtNetBufPeek(buffer,0,request,n)==n);size_t header=0;
    for(size_t i=3;i<n;i++)if(!memcmp(request+i-3,"\r\n\r\n",4)){header=i+1;break;}
    if(!header)return;
    static const char prefix[]="CONNECT 127.0.0.1:443 HTTP/1.1\r\n";
    CHECK(header>=sizeof(prefix)-1&&!memcmp(request,prefix,sizeof(prefix)-1));
    CHECK(xrtNetStreamConsume(stream,header)==header);
    static const char reply[]="HTTP/1.1 200 Connection Established\r\n\r\n";
    CHECK(xrtNetStreamSend(stream,reply,sizeof(reply)-1)==XNET_RESULT_OK);
    XL6_TlsServerContextV1* role=peer->role;xtlsserverconfig config;xrtTlsServerConfigInit(&config);
    config.Context=role->parameters;config.Identity=role->identity;
    xtlssession* session=xrtTlsServerCreate(&config,NULL);CHECK(session);
    xtlsstreamevents events={0};events.Close=proxyClose;xtlsstream* attached=NULL;
    CHECK(xrtTlsStreamAttach(stream,session,NULL,&events,peer,&attached)&&attached);
    xrtAtomicPtrStore(&peer->tls,attached,XMEMORY_RELEASE);xrtAtomic32Store(&peer->accepted,1,XMEMORY_RELEASE);
}
static void peerWait(const xatomic32* value)
{
    XL6_DeadlineV1 due=xl6DeadlineAfterMillisecondsV1(10000);
    while(!xrtAtomic32Load(value,XMEMORY_ACQUIRE)){CHECK(!xl6DeadlineExpiredV1(due));xrtThreadYield();}
}
static xnetproxy* proxyStart(TlsProxyPeer* peer,xnetportbackend backend)
{
    void* client=NULL;contexts(&client,&peer->role);xl6TlsClientContextDestroyV1(client);
    xnetengineconfig engine;xrtNetEngineConfigInit(&engine);engine.Workers=1;engine.Backend=backend;
    peer->engine=xrtNetEngineCreate(&engine);CHECK(peer->engine&&xrtNetEngineStart(peer->engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    xnetlistenerevents events={0};events.Accept=proxyAccept;events.Close=proxyListenerClosed;
    xnetstreamevents stream={0};stream.Read=proxyRead;
    peer->listener=xrtNetListen(peer->engine,&listen,&events,&stream,peer);CHECK(peer->listener);
    xnetaddr bound;CHECK(xrtNetListenerLocal(peer->listener,&bound));
    xnetproxyconfig config;xrtNetProxyConfigInit(&config);config.Type=XNET_PROXY_HTTP_CONNECT;
    config.Host=(xstrview){"127.0.0.1",9};config.Port=bound.Port;
    xnetproxy* proxy=xrtNetProxyCreate(&config);CHECK(proxy);return proxy;
}
static void proxyStop(TlsProxyPeer* peer)
{
    xtlsstream* stream=xrtAtomicPtrLoad(&peer->tls,XMEMORY_ACQUIRE);CHECK(stream);
    CHECK(xrtTlsStreamAbort(stream));peerWait(&peer->closed);
    CHECK(xrtNetListenerClose(peer->listener));peerWait(&peer->listenerClosed);
    xrtTlsStreamDestroy(stream);xrtNetListenerDestroy(peer->listener);
    XL6_DeadlineV1 due=xl6DeadlineAfterMillisecondsV1(10000);
    for(;;){xnetretireresult state=xrtNetEngineTryDestroy(peer->engine);if(state==XNET_RETIRE_READY)break;
        CHECK(state==XNET_RETIRE_BUSY&&!xl6DeadlineExpiredV1(due));xrtThreadYield();}
    xl6TlsServerContextDestroyV1(peer->role);
}
