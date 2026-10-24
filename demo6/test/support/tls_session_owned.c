/* Storage/lifecycle fixture only. Real normal TLS handshakes are separate.
 * Fixture helpers are defined AFTER the genuine XRT single implementation,
 * so no native layout is duplicated or guessed in this resident test unit. */
#include "src/runtime/xl6_tls_session_internal.h"
#include "src/runtime/xl6_tls_session_abi.h"
#include "src/runtime/xl6_net_buffer_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
xtlssession* fixtureSessionCreate(void);
unsigned fixtureSessionDestroyed(void);
void fixtureSessionPlain(xtlssession*,const void*,size_t);
void fixtureSessionCipher(xtlssession*,xnetbytes*,unsigned);
void fixtureSessionPlainBytes(xtlssession*,xnetbytes*,unsigned);
void fixtureSessionFeedBytes(xtlssession*,xnetbytes*);
void fixtureSessionOpaque(xtlssession*,void*,xnetreleaseproc);
static unsigned budgets,failed,published,constructors;
static xtlsresult noDrive(xtlssession* native){(void)native;return XTLS_AGAIN;}
static const XL6_TlsSessionRoleOpsV1 fixtureOperations={noDrive,NULL,NULL,NULL,NULL,NULL};
static XL6_TlsSessionV1* make(void)
{XL6_TlsSessionV1* self=xl6TlsSessionTakeInternalV1(fixtureSessionCreate(),&fixtureOperations);assert(self&&!xrtGetError());return self;}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount&&before.OverflowCount==after.OverflowCount&&
        before.UnderflowCount==after.UnderflowCount);
}
static bool bytesEqual(xvalue* value,const void* data,size_t size)
{xbytesview view={0};return value&&xrtValueGetBytes(value,&view)&&view.Size==size&&(!size||!memcmp(view.Data,data,size));}
static void contentHash(xvalue* value,const void* data,size_t size)
{
    xvalue* expected=xrtValueBytes((xbytesview){data,size});assert(expected);uint64 actual=0,wanted=0;
    assert(xrtValueHash(value,&actual)&&xrtValueHash(expected,&wanted)&&actual==wanted);xrtValueRelease(expected);
}
static size_t count(xrtownershipref ref)
{size_t n=0;assert(ref.Ops->Count(ref.Data,&n));return n;}
typedef struct Edges {xrtownershipref expected;unsigned count;} Edges;
static bool visit(xrtownershipref ref,void* raw)
{Edges* edges=raw;assert(ref.Data==edges->expected.Data&&ref.Ops==edges->expected.Ops);edges->count++;return true;}
static void releaseOpaque(void* raw,cbytes data,size_t size)
{assert(data&&size==3);(*(unsigned*)raw)++;}
static XL6_OwnershipCatalog* catalog(void)
{
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainTlsSessionV1Get(),xl6OwnershipDomainNetBufferNativeV1Get()};
    XL6_OwnershipCatalog* result=xl6OwnershipCatalogCreateV1(domains,2);assert(result);return result;
}
static void entries(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned deaths=fixtureSessionDestroyed();
    XL6_TlsSessionV1* self=make();void* slot=self;
    xvalue* handle=xl6NativeHandleTakeV1(&slot,xl6TlsSessionHandlePolicyV1Get());assert(handle&&!slot);
    XL6_OwnershipCatalog* policies=catalog();XL6_TlsSessionScopeV1 scope={0},other={0};
    assert(xl6TlsSessionHoldInternalV1(self)&&xl6TlsSessionEnterInternalV1(self,true,&scope));
    assert(!xl6TlsSessionEnterInternalV1(self,true,&other)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xrtownershipref ref=xl6TlsSessionOwnershipV1(self);xrtownershipscope freeze={0};
    assert(xrtOwnershipFreezeTryBegin(&freeze));size_t unchanged=123;
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* completion=NULL;
    assert(xl6TlsSessionQuiescenceV1Get()->Busy(self)&&!ref.Ops->Count(self,&unchanged)&&unchanged==123);
    assert(xl6OwnershipCatalogProbeV1(ref,policies,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_BUSY);
    assert(xrtOwnershipScopeEnd(&freeze));
    xrtValueRelease(handle);xl6TlsSessionDropInternalV1(self);
    assert(fixtureSessionDestroyed()==deaths); /* Only the real call reference remains. */
    xl6TlsSessionLeaveInternalV1(&scope);assert(fixtureSessionDestroyed()==deaths+1);
    xl6OwnershipCatalogDestroyV1(policies);balanced(before);
}
static void graph(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TlsSessionV1* self=make();
    xnetbytes* data=xrtNetBytesCopy("a\0b",3);assert(data);fixtureSessionFeedBytes(self->native,data);
    xrtownershipref ref=xl6TlsSessionOwnershipV1(self),payload=xrtNetBytesOwnership(data);
    XL6_OwnershipCatalog* policies=catalog();xrtownershipscope freeze={0};int token=0;
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* completion=NULL;
    assert(xrtOwnershipFreezeTryBegin(&freeze));Edges edges={payload,0};
    assert(count(ref)==1&&count(payload)==2&&ref.Ops->Trace(ref.Data,visit,&edges)&&edges.count==1);
    assert(xl6OwnershipCatalogProbeV1(ref,policies,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_ADMITTED&&adapter&&!preparation&&!completion);
    assert(adapter->Hold(self)&&adapter->Claim(self,&token)&&count(ref)==2);
    assert(!xl6TlsSessionHoldInternalV1(self));adapter->Restore(self,&token);
    assert(adapter->Claim(self,&token));const xrtownershipadapterv1* child=xrtNetBytesOwnershipAdapterV1(payload);assert(child&&child->Hold(data));
    adapter->Clear(self,&token);assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(self,&token)&&adapter->Finish(self,&token));
    xl6TlsSessionDropInternalV1(self);adapter->Drop(self);
    assert(xrtOwnershipFreezeTryBegin(&freeze)&&count(payload)==2&&xrtOwnershipScopeEnd(&freeze));
    xrtNetBytesDestroy(data);child->Drop(data);xl6OwnershipCatalogDestroyV1(policies);balanced(before);
}
static void opaque(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TlsSessionV1* self=make();unsigned releases=0;
    fixtureSessionOpaque(self->native,&releases,releaseOpaque);xrtownershipscope freeze={0};Edges edges={0};
    xrtownershipref ref=xl6TlsSessionOwnershipV1(self);assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(!ref.Ops->Trace(ref.Data,visit,&edges)&&!releases&&!edges.count);assert(xrtOwnershipScopeEnd(&freeze));
    xl6TlsSessionDestroyV1(self);assert(releases==1);balanced(before);
}
static void outputs(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TlsSessionV1* self=make();
    xnetbytes* data=xrtNetBytesCopy("a\0b",3);assert(data);fixtureSessionCipher(self->native,data,20);
    xvalue* first=xl6TlsSessionTakeCipherV1(self,1000);xbytesview view={0};assert(first&&xrtValueGetBytes(first,&view)&&view.Size==48);
    assert(xl6TlsSessionPendingCipherV1(self)==12);xvalue* second=xl6TlsSessionTakeCipherV1(self,1000);
    assert(second&&xrtValueGetBytes(second,&view)&&view.Size==12&&xl6TlsSessionPendingCipherV1(self)==0);
    for(size_t i=0;i<view.Size;i+=3)assert(!memcmp(view.Data+i,"a\0b",3));
    uint8 expected[48];for(size_t i=0;i<sizeof(expected);i+=3)memcpy(expected+i,"a\0b",3);
    contentHash(first,expected,48);contentHash(second,expected,12);
    xrtValueRelease(first);xrtValueRelease(second);xrtNetBytesDestroy(data);
    xl6TlsSessionDisposeV1(self);xl6TlsSessionDisposeV1(self);assert(xl6TlsSessionDisposedV1(self));
    assert(!xl6TlsSessionPendingPlainV1(self)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xl6TlsSessionDestroyV1(self);balanced(before);
}
static void plaintextBatches(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TlsSessionV1* self=make();
    xnetbytes* data=xrtNetBytesCopy("a\0b",3);assert(data);fixtureSessionPlainBytes(self->native,data,20);
    XL6_ProductI64BytesV1 first=xl6TlsSessionReadV1(self,1000);assert(first.e0==XTLS_OK&&xl6TlsSessionPendingPlainV1(self)==12);
    XL6_ProductI64BytesV1 second=xl6TlsSessionReadV1(self,1000);assert(second.e0==XTLS_OK&&xl6TlsSessionPendingPlainV1(self)==0);
    uint8 expected[48];for(size_t i=0;i<sizeof(expected);i+=3)memcpy(expected+i,"a\0b",3);
    assert(bytesEqual(first.e1,expected,48)&&bytesEqual(second.e1,expected,12));
    contentHash(first.e1,expected,48);contentHash(second.e1,expected,12);
    assert(!memcmp(xrtNetBytesView(data).Data,"a\0b",3));
    xrtValueRelease(first.e1);xrtValueRelease(second.e1);xrtNetBytesDestroy(data);xl6TlsSessionDestroyV1(self);balanced(before);
}
static void failures(bool plain)
{
    bool completed=false;
    for(unsigned budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TlsSessionV1* self=make();
        xnetbytes* data=xrtNetBytesCopy("a\0b",3);assert(data);
        if(plain)fixtureSessionPlain(self->native,"a\0b",3);else fixtureSessionCipher(self->native,data,1);
        assert(xrtMemDebugFailAfter(budget));xvalue* outputValue=NULL;
        if(plain){XL6_ProductI64BytesV1 result=xl6TlsSessionReadV1(self,20);outputValue=result.e1;
            if(result.e1)assert(result.e0==XTLS_OK);}
        else outputValue=xl6TlsSessionTakeCipherV1(self,20);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();budgets++;
        assert(hit?(!outputValue&&xrtGetError()):(outputValue&&!xrtGetError()));xrtClearError();
        assert((plain?xl6TlsSessionPendingPlainV1(self):xl6TlsSessionPendingCipherV1(self))==(hit?3u:0u));
        if(hit)failed++;else {assert(bytesEqual(outputValue,"a\0b",3));completed=true;}
        xrtValueRelease(outputValue);xrtNetBytesDestroy(data);xl6TlsSessionDestroyV1(self);balanced(before);if(completed)break;
    }assert(completed);
}
static void construction(void)
{
    bool completed=false;
    for(unsigned budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xtlssession* native=fixtureSessionCreate();unsigned deaths=fixtureSessionDestroyed();
        assert(xrtMemDebugFailAfter(budget));XL6_TlsSessionV1* self=xl6TlsSessionTakeInternalV1(native,&fixtureOperations);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();constructors++;assert(hit?(!self&&xrtGetError()):(self&&!xrtGetError()));
        xrtClearError();xl6TlsSessionDestroyV1(self);assert(fixtureSessionDestroyed()==deaths+1);balanced(before);if(!hit){completed=true;break;}
    }assert(completed);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned deaths=fixtureSessionDestroyed();
    assert(!xl6TlsSessionTakeInternalV1(fixtureSessionCreate(),NULL)&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    assert(fixtureSessionDestroyed()==deaths+1);xrtClearError();XL6_TlsSessionV1* self=make();
    static const XL6_TlsSessionRoleOpsV1 missingDrive={0};
    unsigned rejected=fixtureSessionDestroyed();
    assert(!xl6TlsSessionTakeInternalV1(fixtureSessionCreate(),&missingDrive)&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    assert(fixtureSessionDestroyed()==rejected+1);xrtClearError();
    xrtSetErrorKind(XERR_VALUE);const xerror* original=xrtGetError();assert(original);
    xl6TlsSessionDestroyV1(self);assert(xrtGetError()==original);balanced(before);
}
static void observations(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TlsSessionV1* self=make();
    assert(xl6TlsSessionPeerCertificateCountV1(self)==0&&!xrtGetError());
    XL6_ProductBoolI64I64V1 alert=xl6TlsSessionPeerAlertV1(self);
    assert(!alert.e0&&!alert.e1&&!alert.e2&&!xrtGetError());
    assert(!xl6TlsSessionPeerCertificateAtV1(self,0)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    assert(!xl6TlsSessionServerNameV1(self)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    assert(!xl6TlsSessionServerCookieV1(self)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    assert(xl6TlsSessionKeyUpdateV1(self,true)==XTLS_ERROR&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);xrtClearError();
    /* Every error entry releases its actual pin and gate. */
    assert(self->refs==1&&!xrtAtomic32Load(&self->gate,XMEMORY_ACQUIRE));
    xl6TlsSessionDisposeV1(self);
    assert(!xl6TlsSessionPeerAlertV1(self).e0&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xl6TlsSessionDestroyV1(self);balanced(before);
}
static void emptyFailure(void)
{
    bool completed=false;
    for(unsigned budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TlsSessionV1* self=make();assert(xrtMemDebugFailAfter(budget));
        XL6_ProductI64BytesV1 result=xl6TlsSessionReadV1(self,20);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();budgets++;
        assert(hit?(!result.e1&&xrtGetError()):(result.e0==XTLS_AGAIN&&bytesEqual(result.e1,NULL,0)));xrtClearError();
        assert(xl6TlsSessionPendingPlainV1(self)==0);if(hit)failed++;else completed=true;
        xrtValueRelease(result.e1);xl6TlsSessionDestroyV1(self);balanced(before);if(completed)break;
    }assert(completed);
}
static void publication(void)
{
    bool completed=false;
    for(unsigned budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* slot=make();unsigned deaths=fixtureSessionDestroyed();
        assert(xrtMemDebugFailAfter(budget));xvalue* value=xl6NativeHandleTakeV1(&slot,xl6TlsSessionHandlePolicyV1Get());
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();published++;assert(!slot&&(hit?!value:value!=NULL));
        xrtClearError();xrtValueRelease(value);assert(fixtureSessionDestroyed()==deaths+1);balanced(before);if(!hit){completed=true;break;}
    }assert(completed);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));entries();graph();opaque();outputs();plaintextBatches();construction();observations();failures(false);failures(true);emptyFailure();publication();
    assert(xrtMemDebugReset());printf("TLS managed session owners: %u output budgets; %u failures; %u publication budgets; %u construction budgets; active last-drop, Busy, graph finish, short takes and byte balance\n",budgets,failed,published,constructors);return 0;
}
