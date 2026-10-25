#include "include/xruntime_decl.h"
#include "src/runtime/xl6_tls_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);
}
static bool bytesEqual(const xvalue* value,const char* expected,size_t size)
{xbytesview view={0};return value&&xrtValueGetBytes(value,&view)&&view.Size==size&&(!size||!memcmp(view.Data,expected,size));}
static void exports(size_t point,bool binding)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xtlsresumeconfig config={0};config.Version=XTLS_VERSION_13;config.Cipher=XTLS_AES_128_GCM_SHA256;
    config.Ticket=(xbytesview){(const uint8*)"t\0b",3};
    config.Secret=(xbytesview){(const uint8*)"01234567890123456789012345678901",32};
    config.Lifetime=120;config.IssuedAt=1000000000;config.ServerName=(xstrview){"localhost",9};
    void* raw=xrtTlsResumeCreate(&config);assert(raw);
    assert(xrtMemDebugFailAfter(point));
    xvalue* t=NULL,*s=NULL,*p=NULL,*i=NULL,*handle=NULL;xstrview n={0};
    if(binding)handle=xl6NativeHandleTakeV1(&raw,xl6TlsResumeHandlePolicyV1Get());
    else{t=xl6TlsResumeTicketV1(raw);s=xl6TlsResumeExportSecretV1(raw);p=xl6TlsResumeProtocolV1(raw);i=xl6TlsResumePeerIdentityV1(raw);n=xl6TlsResumeServerNameV1(raw);}
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(binding){assert(!raw);assert(hit?!handle:handle!=NULL);}
    else if(!hit)assert(t&&s&&p&&i&&n.Data);
    xrtClearError();xrtValueRelease(handle);xl6TlsResumeDestroyV1(raw);
    xrtValueRelease(t);xrtValueRelease(s);xrtValueRelease(p);xrtValueRelease(i);xrtFree((void*)n.Data);
    assert(!xrtGetError());balanced(&before);
}
static void roundTrip(bool failing,size_t point)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xvalue* ticket=xrtValueBytes((xbytesview){(const uint8*)"t\0\xff",3});
    xvalue* secret=xrtValueBytes((xbytesview){(const uint8*)"01234567890123456789012345678901",32});
    xvalue* protocol=xrtValueBytes((xbytesview){(const uint8*)"x\0\xff",3});
    xvalue* identity=xrtValueBytes((xbytesview){(const uint8*)"peer\0id",7});
    assert(ticket&&secret&&protocol&&identity);
    char name[]="localhost";
    if(failing)assert(xrtMemDebugFailAfter(point));
    void* raw=xl6TlsResumeFromPartsV1(772,4865,ticket,secret,1000000000,120,UINT32_MAX,
        (xstrview){name,9},protocol,identity,0);
    if(failing){bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(hit?!raw:raw!=NULL);xrtClearError();}
    xrtValueRelease(ticket);xrtValueRelease(secret);xrtValueRelease(protocol);xrtValueRelease(identity);
    memset(name,'x',9);
    if(raw){
        assert(xl6TlsResumeVersionV1(raw)==772&&xl6TlsResumeCipherV1(raw)==4865);
        assert(xl6TlsResumeLifetimeV1(raw)==120&&xl6TlsResumeAgeAddV1(raw)==UINT32_MAX&&xl6TlsResumeMaxEarlyDataV1(raw)==0);
        assert(xl6TlsResumeIssuedAtV1(raw)==1000000000&&xl6TlsResumeExpiresAtV1(raw)==1120000000);
        assert(xl6TlsResumeValidAtV1(raw,1000000000)&&!xl6TlsResumeValidAtV1(raw,999999999)&&!xl6TlsResumeValidAtV1(raw,1120000000));
        assert(!xrtGetError()&&xl6TlsResumeTicketAgeV1(raw,1001234000)==1233);
        xvalue* t=xl6TlsResumeTicketV1(raw),*s=xl6TlsResumeExportSecretV1(raw),*p=xl6TlsResumeProtocolV1(raw),*i=xl6TlsResumePeerIdentityV1(raw);
        xstrview n=xl6TlsResumeServerNameV1(raw);assert(n.Size==9&&!memcmp(n.Data,"localhost",9));
        assert(bytesEqual(t,"t\0\xff",3)&&bytesEqual(s,"01234567890123456789012345678901",32));
        assert(bytesEqual(p,"x\0\xff",3)&&bytesEqual(i,"peer\0id",7));
        /* A detached export survives final credential release. */
        assert(xl6TlsResumeTicketAgeV1(raw,1120000000)==0&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
        const XL6_NativeHandlePolicyV1* policy=xl6TlsResumeHandlePolicyV1Get();
        xvalue* handle=xl6NativeHandleTakeV1(&raw,policy);assert(handle&&!raw);
        void* native=NULL;assert(xrtValueGetHandle(handle,&native,NULL,NULL)&&native);
        assert(policy->Trace(handle,(xrtownershipvisitor)NULL,NULL)==false);
        xrtValueRelease(handle);
        assert(bytesEqual(t,"t\0\xff",3)&&bytesEqual(s,"01234567890123456789012345678901",32));
        xrtValueRelease(t);xrtValueRelease(s);xrtValueRelease(p);xrtValueRelease(i);xrtFree((void*)n.Data);
    }
    assert(!xrtGetError());balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    roundTrip(false,0);
    for(size_t point=0;point<128;++point){roundTrip(true,point);exports(point,false);exports(point,true);}
    puts("TLS resume: owned binary exports, exact microsecond time, millisecond wire age, 384 OOM prefixes: ok");
    return 0;
}
