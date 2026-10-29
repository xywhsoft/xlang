#include "include/xruntime_decl.h"
#include "src/runtime/xl6_tls_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool equal(xvalue* value,const char* expected,size_t size)
{xbytesview v={0};return value&&xrtValueGetBytes(value,&v)&&v.Size==size&&!memcmp(v.Data,expected,size);}
static void roundTrip(size_t point)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    uint8 protocols[]={0,7,2,'h','2',3,'x',0,255},name[]="localhost",ticket[]={'t',0,255};
    xtlsserverresumerequest input={{name,9},{protocols,sizeof(protocols)},{ticket,sizeof(ticket)},UINT32_MAX};
    assert(xrtMemDebugFailAfter(point));void* raw=xl6TlsResumeRequestCopyV1(&input);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(hit?!raw:raw!=NULL);xrtClearError();
    if(raw){
        memset(name,'?',9);memset(protocols,0,sizeof(protocols));memset(ticket,0,sizeof(ticket));
        assert(xl6TlsResumeRequestProtocolCountV1(raw)==2&&xl6TlsResumeRequestAgeV1(raw)==UINT32_MAX);
        xvalue* n=xl6TlsResumeRequestNameV1(raw),*p=xl6TlsResumeRequestProtocolAtV1(raw,1),*t=xl6TlsResumeRequestTicketV1(raw);
        assert(equal(n,"localhost",9)&&equal(p,"x\0\xff",3)&&equal(t,"t\0\xff",3));
        assert(!xl6TlsResumeRequestProtocolAtV1(raw,2)&&xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6TlsResumeRequestHandlePolicyV1Get());assert(handle&&!raw);xrtValueRelease(handle);
        assert(equal(n,"localhost",9)&&equal(p,"x\0\xff",3)&&equal(t,"t\0\xff",3));xrtValueRelease(n);xrtValueRelease(p);xrtValueRelease(t);
    }
    xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&!xrtGetError());
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    for(size_t point=0;point<64;++point)roundTrip(point);
    puts("TLS resume request: full owned bytes and ALPN, uint32 age, 64 OOM prefixes balanced: ok");return 0;
}
