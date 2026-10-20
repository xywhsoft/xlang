/* Ordinary local resource concurrency only. Opens unbound UDP sockets; never
 * sends, receives, binds, resolves names or connects to any endpoint. Build
 * with net_socket + thread + atomic + time, optionally TEST_SOCKET_CACHE_HEADER. */
#if defined(TEST_SOCKET_CACHE_HEADER)
#define XRT_IMPLEMENTATION
#include TEST_SOCKET_CACHE_HEADER
#else
#include <xrt.h>
#endif
#if !defined(XRT_SINGLE_HEADER)
#include "src/internal/xrt_net_socket.h"
#endif
#include <assert.h>
#include <stdio.h>

enum { WORKERS=8 };
typedef struct Shared {
    xnetsocket socket;
    xatomic32 ready,go;
} Shared;
typedef struct Item {
    Shared* shared;
    uintptr_t receive,send;
    uint32 metadata,control,capabilities;
} Item;
static int32 worker(ptr raw)
{
    Item* item=raw;Shared* shared=item->shared;
    xrtAtomic32FetchAdd(&shared->ready,1,XMEMORY_RELEASE);
    double end=xrtTimer()+5.0;
    while(!xrtAtomic32Load(&shared->go,XMEMORY_ACQUIRE)){
        assert(xrtTimer()<end);xrtThreadYield();
    }
    item->receive=__xrtNetSocketReceiveMessage(shared->socket);
    item->send=__xrtNetSocketSendMessage(shared->socket);
    item->metadata=xrtNetSocketDgramMetaAvailable(shared->socket);
    item->control=xrtNetSocketDgramControlAvailable(shared->socket);
    item->capabilities=xrtNetSocketDgramCapabilities(shared->socket);
    assert(!xrtGetError());
    return 0;
}
static void exercise(xnetfamily family)
{
    Shared shared={0};Item items[WORKERS];xthread* threads[WORKERS];
    shared.socket=xrtNetSocketOpen(family,XNET_SOCKET_DGRAM,XNET_SOCKET_NONBLOCK);
    assert(shared.socket);
    for(unsigned i=0;i<WORKERS;++i){
        items[i]=(Item){0};items[i].shared=&shared;
        threads[i]=xrtThreadCreate(worker,&items[i],0);assert(threads[i]);
    }
    double end=xrtTimer()+5.0;
    while(xrtAtomic32Load(&shared.ready,XMEMORY_ACQUIRE)!=WORKERS){
        assert(xrtTimer()<end);xrtThreadYield();
    }
    xrtAtomic32Store(&shared.go,1,XMEMORY_RELEASE);
    for(unsigned i=0;i<WORKERS;++i){
        assert(xrtThreadWaitFor(threads[i],5000)==XWAIT_OK);
        assert(xrtThreadExitCode(threads[i])==0);xrtThreadDestroy(threads[i]);
        assert(items[i].receive==items[0].receive&&items[i].send==items[0].send);
        assert(items[i].metadata==items[0].metadata&&items[i].control==items[0].control&&
            items[i].capabilities==items[0].capabilities);
        #if defined(_WIN32) || defined(_WIN64)
        assert(items[i].receive&&items[i].send);
        #else
        assert(!items[i].receive&&!items[i].send);
        #endif
    }
    assert(xrtNetSocketClose(shared.socket));
}
int main(void)
{
    exercise(XNET_FAMILY_IPV4);exercise(XNET_FAMILY_IPV6);
    puts("Socket message cache: two unbound address families, eight simultaneous first users, canonical publication; no traffic");
    return 0;
}
