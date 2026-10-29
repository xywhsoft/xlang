/* Offline ownership test only: no socket, TLS record or handshake executes. */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_BUFFER
#define XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_DEFLATE
#define XRT_MODULE_INFLATE
#define XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#define XRT_MODULE_TLS_STREAM_FUTURE
#define XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_SCHEDULE_SHA384
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_KEY_EXCHANGE_X448
#define XRT_MODULE_TLS_KEY_EXCHANGE_P256
#define XRT_MODULE_TLS_KEY_EXCHANGE_P384
#define XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_TLS_RECORD_CHACHA
#define XRT_MODULE_NET_TCP_DIAL_SYNC
#define XRT_MODULE_UNICODE
#define XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_STRING
#define XRT_MODULE_TIME_TEXT
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#define XMAIL_MODULE_XMAIL
#include "release/extlibs/xmail/include/xmail.h"
static unsigned calls;
static bool noHandshake(xnetstream* tcp,const xtlsclientconfig* client,
    const xtlsstreamconfig* stream,const xtlsstreamevents* events,ptr data,xtlsstream** output)
{
    (void)tcp;(void)client;(void)stream;(void)events;(void)data;(void)output;
    ++calls;xrtSetErrorKind(XERR_STATE);return false;
}
#define xrtTlsStreamClient noHandshake
#include "release/extlibs/xmail/src/native/transport/mail_net_tls.c"
#undef xrtTlsStreamClient
#include "release/extlibs/xmail/src/xlang/xmail_owned_post.c"
/* Creation-failure double isolates the dispatcher's ownership rollback;
 * the real descriptor, graph, OOM and task residency have a separate suite. */
void* xl6ForeignGraphCreateV1(void* raw,const XL6_ForeignGraphDescriptorV1* descriptor,const xvalue* release)
{(void)raw;(void)descriptor;(void)release;xrtSetErrorKind(XERR_MEMORY);return NULL;}
void xl6ForeignGraphDestroyV1(void* raw){(void)raw;abort();}
const xnettaskownershipv1* xl6ForeignGraphTaskPolicyV1Get(void){abort();}
static __xmailtlsupgrade* request(xfuture** future)
{
    __xmailtlsupgrade* p=xrtCalloc(1,sizeof(*p));assert(p);
    xrtAtomic32Init(&p->Refs,2);xrtAtomic32Init(&p->Gate,0);
    p->Promise=xrtPromiseCreate(future,NULL);assert(p->Promise);return p;
}
int main(void)
{
    (void)&xmail_post_owned;
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before,after;
    xrtMemDebugSnapshot(&before);xfuture* future;
    __xmailtlsupgrade* p=request(&future);xrtAtomic32Store(&p->Gate,2,XMEMORY_RELEASE);
    p->Client=(void*)1;p->Stream=(void*)1;p->Tcp=(void*)1;
    __xrtMailUpgradeDrop(p);__xrtMailNetTlsUpgradeTask(NULL,p);
    assert(calls==0);assert(xrtFutureState(future)!=XFUTURE_PENDING);xrtFutureDestroy(future);
    p=request(&future);__xrtMailNetTlsUpgradeTask(NULL,p);
    assert(calls==1&&xrtFutureState(future)==XFUTURE_FAILED);
    assert(xrtAtomic32Load(&p->Refs,XMEMORY_ACQUIRE)==1);
    xrtFutureDestroy(future);__xrtMailUpgradeDrop(p);xrtClearError();
    p=request(&future);xmail_owned_post* post=xrtCalloc(1,sizeof(*post));assert(post);
    post->data=p;post->promise=p->Promise;post->discard=__xrtMailUpgradeDrop;
    xmail_post_destroy(post);assert(xrtAtomic32Load(&p->Refs,XMEMORY_ACQUIRE)==1);
    assert(xrtFutureState(future)!=XFUTURE_PENDING);__xrtMailUpgradeDrop(p);xrtFutureDestroy(future);
    for(unsigned budget=0;budget<2;++budget){
        xrtClearError();p=request(&future);
        assert(xrtMemDebugFailAfter(budget));
        assert(!xmail_post_owned(NULL,__xrtMailNetTlsUpgradeTask,p,p->Promise,__xrtMailUpgradeDrop,NULL));
        xrtMemDebugFailClear();assert(xrtGetError());xrtClearError();
        assert(xrtAtomic32Load(&p->Refs,XMEMORY_ACQUIRE)==1);
        assert(xrtFutureState(future)!=XFUTURE_PENDING);
        xrtFutureDestroy(future);__xrtMailUpgradeDrop(p);
    }
    xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(after.InvalidFreeCount==before.InvalidFreeCount&&after.DoubleFreeCount==before.DoubleFreeCount);
    assert(xrtMemDebugReset());puts("mail handoff lifetime: abandoned, published, queue-dropped; balanced");
    return 0;
}
