/* Ordinary owned-packet and allocator contracts. No socket, handshake,
 * certificate verification or malformed crypto/security campaign. */
#include "src/runtime/xl6_tls_server_request.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8 name[]={'t','e','n','a','n','t','.','e','x','a','m','p','l','e'};
static const uint8 alpn[]={0,9,2,'h','2',3,'a',0,'b',1,0xff};
static const uint8 binaryProtocol[]={'a',0,'b'};

static void bytesEqual(const xvalue* value,const uint8* data,size_t size)
{
    xbytesview view={0};assert(value&&xrtValueGetBytes(value,&view));
    assert(view.Size==size&&(!size||!memcmp(view.Data,data,size)));
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount);
    assert(before->DoubleFreeCount==after.DoubleFreeCount);
    assert(before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static XL6_TlsServerRequestV1* sample(void)
{
    /* Overwrite both borrowed inputs after Copy. All observations must then
     * come from the private packet, including the per-protocol view table. */
    uint8 borrowedName[sizeof(name)],borrowedAlpn[sizeof(alpn)];
    memcpy(borrowedName,name,sizeof(name));memcpy(borrowedAlpn,alpn,sizeof(alpn));
    XL6_TlsServerRequestV1* request=xl6TlsServerRequestCopyV1(
        (xbytesview){borrowedName,sizeof(name)},(xbytesview){borrowedAlpn,sizeof(alpn)});
    memset(borrowedName,0,sizeof(borrowedName));memset(borrowedAlpn,0,sizeof(borrowedAlpn));
    return request;
}
static void detached(void)
{
    xmemdebugsnapshot before,created;xrtMemDebugSnapshot(&before);
    XL6_TlsServerRequestV1* request=sample();assert(request);
    xrtMemDebugSnapshot(&created);
    assert(created.AllocCount-before.AllocCount==1); /* One allocation, not N+1. */
    assert(xl6TlsServerRequestProtocolCountV1(request)==3);
    xvalue* copyName=xl6TlsServerRequestNameV1(request);
    xvalue* copies[3];for(int i=0;i<3;++i)copies[i]=xl6TlsServerRequestProtocolAtV1(request,i);
    xl6TlsServerRequestDestroyV1(request);
    bytesEqual(copyName,name,sizeof(name));bytesEqual(copies[0],alpn+3,2);
    bytesEqual(copies[1],binaryProtocol,sizeof(binaryProtocol));bytesEqual(copies[2],alpn+10,1);
    xrtValueRelease(copyName);for(int i=0;i<3;++i)xrtValueRelease(copies[i]);
    balanced(&before);
}
static void prefix(unsigned operation,size_t point)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_TlsServerRequestV1* request=operation?sample():NULL;xvalue* result=NULL;
    assert(!operation||request);
    assert(xrtMemDebugFailAfter(point));
    if(!operation)request=sample();
    else if(operation==1)result=xl6TlsServerRequestNameV1(request);
    else result=xl6TlsServerRequestProtocolAtV1(request,1);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(!point)assert(hit); /* Each allocation-bearing operation was faulted. */
    if(hit){assert(xrtGetError());assert(operation?!result:!request);}
    else if(!operation)assert(request&&xl6TlsServerRequestProtocolCountV1(request)==3);
    else if(operation==1)bytesEqual(result,name,sizeof(name));
    else bytesEqual(result,binaryProtocol,sizeof(binaryProtocol));
    xrtClearError();xl6TlsServerRequestDestroyV1(request);xrtValueRelease(result);
    assert(!xrtGetError());balanced(&before);
}
static void boundaries(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_TlsServerRequestV1* empty=xl6TlsServerRequestCopyV1((xbytesview){0},(xbytesview){0});
    assert(empty&&xl6TlsServerRequestProtocolCountV1(empty)==0);
    xvalue* emptyName=xl6TlsServerRequestNameV1(empty);bytesEqual(emptyName,NULL,0);xrtValueRelease(emptyName);
    assert(!xl6TlsServerRequestProtocolAtV1(empty,0)&&xrtGetError());xrtClearError();
    assert(!xl6TlsServerRequestProtocolAtV1(empty,-1)&&xrtGetError());xrtClearError();
    assert(!xl6TlsServerRequestProtocolAtV1(empty,INT64_MAX)&&xrtGetError());xrtClearError();
    xl6TlsServerRequestDestroyV1(empty);
    XL6_TlsServerRequestV1* nameOnly=xl6TlsServerRequestCopyV1((xbytesview){name,sizeof(name)},(xbytesview){0});
    assert(nameOnly&&xl6TlsServerRequestProtocolCountV1(nameOnly)==0);
    xvalue* value=xl6TlsServerRequestNameV1(nameOnly);bytesEqual(value,name,sizeof(name));xrtValueRelease(value);
    xl6TlsServerRequestDestroyV1(nameOnly);
    XL6_TlsServerRequestV1* alpnOnly=xl6TlsServerRequestCopyV1((xbytesview){0},(xbytesview){alpn,sizeof(alpn)});
    assert(alpnOnly&&xl6TlsServerRequestProtocolCountV1(alpnOnly)==3);
    value=xl6TlsServerRequestNameV1(alpnOnly);bytesEqual(value,NULL,0);xrtValueRelease(value);
    xl6TlsServerRequestDestroyV1(alpnOnly);
    assert(!xl6TlsServerRequestNameV1(NULL)&&xrtGetError());xrtClearError();
    assert(!xl6TlsServerRequestProtocolCountV1(NULL)&&xrtGetError());xrtClearError();
    assert(!xl6TlsServerRequestCopyV1((xbytesview){NULL,1},(xbytesview){0})&&xrtGetError());xrtClearError();
    assert(!xl6TlsServerRequestCopyV1((xbytesview){0},(xbytesview){NULL,1})&&xrtGetError());xrtClearError();
    /* Arithmetic rejection occurs before dereferencing these ABI arguments. */
    assert(!xl6TlsServerRequestCopyV1((xbytesview){name,SIZE_MAX},(xbytesview){0})&&xrtGetError());xrtClearError();
    assert(!xl6TlsServerRequestCopyV1((xbytesview){name,sizeof(name)},(xbytesview){alpn,SIZE_MAX})&&xrtGetError());xrtClearError();
    /* An existing failure is never replaced, even with a bad subsequent input. */
    xrtSetErrorKind(XERR_STATE);const xerror* prior=xrtGetError();assert(prior);
    assert(!xl6TlsServerRequestCopyV1((xbytesview){NULL,1},(xbytesview){0}));
    assert(!xl6TlsServerRequestNameV1(NULL)&&xrtGetError()==prior);
    xl6TlsServerRequestDestroyV1(NULL);assert(xrtGetError()==prior);xrtClearError();
    balanced(&before);
}
static void ordered(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    enum{count=2048,size=2+count*3};uint8 wire[size];
    wire[0]=(uint8)((size-2)>>8);wire[1]=(uint8)(size-2);
    for(unsigned i=0;i<count;++i){wire[2+i*3]=2;wire[3+i*3]=(uint8)(i>>8);wire[4+i*3]=(uint8)i;}
    XL6_TlsServerRequestV1* request=xl6TlsServerRequestCopyV1((xbytesview){name,sizeof(name)},(xbytesview){wire,sizeof(wire)});
    assert(request&&xl6TlsServerRequestProtocolCountV1(request)==count);memset(wire,0,sizeof(wire));
    for(unsigned i=0;i<count;++i){
        uint8 expected[2]={(uint8)(i>>8),(uint8)i};
        xvalue* value=xl6TlsServerRequestProtocolAtV1(request,i);bytesEqual(value,expected,2);xrtValueRelease(value);
    }
    xl6TlsServerRequestDestroyV1(request);balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));detached();boundaries();ordered();
    for(unsigned operation=0;operation<3;++operation)for(size_t point=0;point<32;++point)prefix(operation,point);
    puts("TLS server request: detached name/ALPN; one allocation; 2048 ordered protocols; 96 OOM prefixes balanced");
    return 0;
}
