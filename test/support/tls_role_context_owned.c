/* Ordinary immutable configuration, ownership and resource rollback only. */
#include "src/runtime/xl6_tls_role_context_internal.h"
#include "src/runtime/xl6_x509_store_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tls_role_context_vectors.h"
typedef xtlsidentity* (*Factory)(const xbytesview*,size_t,xbytesview);
static Factory factories[]={xrtTlsIdentityRsa,xrtTlsIdentityP256,xrtTlsIdentityP384,xrtTlsIdentityEd25519};
static size_t budgets,failures,publications;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool memoryCause(const xerror* error)
{for(;error;error=xrtErrorCause(error))if(xrtErrorKind(error)==XERR_MEMORY)return true;return false;}
#define FAULTS(Call,Drop) do{bool complete=false;for(size_t i=0;i<256;i++){ \
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(i)); \
    void* p=(Call);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();budgets++; \
    assert(hit?(!p && memoryCause(xrtGetError())):(p && !xrtGetError()));if(hit)failures++;else complete=true; \
    xrtClearError();Drop(p);balanced(before);if(complete)break;}assert(complete);}while(0)
static xvalue* protocols(size_t count,size_t size)
{
    xvalue* result=xrtValueArray();assert(result);
    for(size_t i=0;i<count;++i){uint8 data[255]={0x78,0,0xff};
        if(size){memset(data,0xa5,size);data[0]=(uint8)i;}
        else if(i>0){data[0]=(uint8)(i-1);data[1]=(uint8)i;}
        assert(xrtValueArrayAppendNew(result,xrtValueBytes((xbytesview){data,size?size:3})));
    }return result;
}
static void publication(void* (*make)(void*),void* argument,const XL6_NativeHandlePolicyV1* policy,const XL6_OwnershipDomainV1* domain)
{
    bool complete=false;
    for(size_t i=0;i<32;++i){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* raw=make(argument);assert(raw && xrtMemDebugFailAfter(i));
        xvalue* value=xl6NativeHandleTakeV1(&raw,policy);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();publications++;
        assert(!raw && (hit?(!value && memoryCause(xrtGetError())):(value && !xrtGetError())));xrtClearError();
        if(value){
            const XL6_OwnershipDomainV1* domains[]={domain};XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
            xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count=0;
            assert(xrtOwnershipFreezeTryBegin(&freeze) && ref.Ops->Count(ref.Data,&count) && count==1);
            const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* finish=NULL;
            assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&prep,&finish) && adapter && !prep && !finish);
            int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
            assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);complete=true;
        }
        xrtValueRelease(value);balanced(before);if(complete)break;
    }assert(complete);
}
static void exercise(size_t i)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xx509store* store=xrtX509StoreCreate();assert(store && xrtX509StoreAdd(store,pathVectors[i].root.Data,pathVectors[i].root.Size)==X509_VALUE);
    void* trust=xl6X509StoreVersionTakeV1(&store,(int64_t)pathVectors[i].root.Size);assert(trust && !store);
    void* policy=xl6TlsVerificationPolicyCreateV1(64,16777216,64,100000,4096,67108864,false);assert(policy);
    void* fixed=xl6TlsVerificationPolicyWithTimeV1(policy,pathTime);assert(fixed);
    void* verifier=xl6TlsVerifierCreatePolicyV1(trust,fixed);assert(verifier);
    xbytesview chain[2];chain[0]=pathVectors[i].leaf;chain[1]=pathVectors[i].mid;
    xtlsidentity* identity=factories[i](chain,2,tlsIdentityKeys[i]);assert(identity);
    xtlscontext* parameters=xrtTlsContextCreate(NULL);assert(parameters);
    xvalue* input=protocols(19,0);
    FAULTS(xl6TlsClientContextDefaultV1(verifier),xl6TlsClientContextDestroyV1);
    FAULTS(xl6TlsServerContextDefaultV1(identity),xl6TlsServerContextDestroyV1);
    FAULTS(xl6TlsClientContextCreateV1(parameters,verifier),xl6TlsClientContextDestroyV1);
    FAULTS(xl6TlsServerContextCreateV1(parameters,identity),xl6TlsServerContextDestroyV1);
    void* originalClient=xl6TlsClientContextDefaultV1(verifier);void* originalServer=xl6TlsServerContextDefaultV1(identity);assert(originalClient && originalServer);
    FAULTS(xl6TlsClientContextWithProtocolsV1(originalClient,input),xl6TlsClientContextDestroyV1);
    FAULTS(xl6TlsServerContextWithProtocolsV1(originalServer,input,true),xl6TlsServerContextDestroyV1);
    void* client=xl6TlsClientContextWithProtocolsV1(originalClient,input);void* server=xl6TlsServerContextWithProtocolsV1(originalServer,input,true);assert(client && server);
    assert(!xl6TlsClientContextProtocolCountV1(originalClient) && !xl6TlsServerContextRequiresProtocolV1(originalServer));
    FAULTS(xl6TlsClientContextWithParametersV1(client,parameters),xl6TlsClientContextDestroyV1);
    FAULTS(xl6TlsClientContextWithVerifierV1(client,verifier),xl6TlsClientContextDestroyV1);
    FAULTS(xl6TlsServerContextWithParametersV1(server,parameters),xl6TlsServerContextDestroyV1);
    FAULTS(xl6TlsServerContextWithIdentityV1(server,identity),xl6TlsServerContextDestroyV1);
    FAULTS(xl6TlsClientContextVerifierV1(client),xl6TlsVerifierDestroyV1);
    FAULTS(xl6TlsClientContextProtocolAtV1(client,0),xrtValueRelease);
    FAULTS(xl6TlsServerContextProtocolAtV1(server,0),xrtValueRelease);
    publication(xl6TlsClientContextDefaultV1,verifier,xl6TlsClientContextHandlePolicyV1Get(),xl6OwnershipDomainTlsClientContextV1Get());
    publication(xl6TlsServerContextDefaultV1,identity,xl6TlsServerContextHandlePolicyV1Get(),xl6OwnershipDomainTlsServerContextV1Get());
    xrtValueRelease(input);xrtTlsContextRelease(parameters);xrtTlsIdentityRelease(identity);xl6TlsVerifierDestroyV1(verifier);
    xl6TlsVerificationPolicyDestroyV1(policy);xl6TlsVerificationPolicyDestroyV1(fixed);xl6X509StoreSnapshotDestroyV1(trust);
    xl6TlsClientContextDestroyV1(originalClient);xl6TlsServerContextDestroyV1(originalServer);
    void* projected=xl6TlsClientContextVerifierV1(client);void* projectedIdentity=xl6TlsServerContextIdentityV1(server);
    void* clientParameters=xl6TlsClientContextParametersV1(client);void* serverParameters=xl6TlsServerContextParametersV1(server);
    xvalue* result=xl6TlsClientContextProtocolAtV1(client,0);assert(result && projected && projectedIdentity && clientParameters && serverParameters);
    assert(xl6TlsClientContextProtocolCountV1(client)==19 && xl6TlsServerContextProtocolCountV1(server)==19 && xl6TlsServerContextRequiresProtocolV1(server));
    xl6TlsClientContextDestroyV1(client);xl6TlsServerContextDestroyV1(server);
    xbytesview bytes={0};assert(xrtValueGetBytes(result,&bytes) && bytes.Size==3 && !memcmp(bytes.Data,"x\0\xff",3));
    xx509cert views[2];for(size_t j=0;j<2;j++)assert(xrtX509Parse(chain[j].Data,chain[j].Size,&views[j]));
    assert(xrtTlsVerifierVerify(((XL6_TlsVerifierV1*)projected)->value,XTLS_SERVER,(xstrview){"localhost",9},views,2));
    assert(xrtTlsIdentityCertificateCount(projectedIdentity)==2 && xrtTlsContextLimits(clientParameters)->FeedLimit==262144);
    xrtValueRelease(result);xl6TlsVerifierDestroyV1(projected);xrtTlsIdentityRelease(projectedIdentity);
    xrtTlsContextRelease(clientParameters);xrtTlsContextRelease(serverParameters);balanced(before);
}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(size_t i=0;i<4;++i)exercise(i);
    xvalue* input=protocols(255,255);uint8 tail[252]={0xff};
    assert(xrtValueArrayAppendNew(input,xrtValueBytes((xbytesview){tail,sizeof(tail)})));
    XL6_TlsProtocolsV1* packet=NULL;assert(xl6TlsProtocolsReadV1(input,&packet) && packet && packet->count==256);
    XL6_TlsProtocolsV1* copy=NULL;assert(xl6TlsProtocolsCopyV1(packet,&copy) && copy && copy->bytes==packet->bytes);
    xrtValueRelease(input);xrtFree(packet);xrtFree(copy);
    xvalue* duplicate=protocols(1,0);assert(xrtValueArrayAppendNew(duplicate,xrtValueBytes((xbytesview){(const uint8*)"x\0\xff",3})));
    packet=NULL;assert(!xl6TlsProtocolsReadV1(duplicate,&packet) && !packet && xrtErrorKind(xrtGetError())==XERR_EXISTS);xrtClearError();xrtValueRelease(duplicate);
    xvalue* empty=xrtValueArray();assert(empty && xl6TlsProtocolsReadV1(empty,&packet) && !packet);xrtValueRelease(empty);
    balanced(before);assert(xrtMemDebugReset());
    printf("TLS role contexts: 4 ordinary families; %zu construction/copy budgets; %zu failures; %zu publication budgets; exact opaque ALPN and 65535-byte envelope; independent native projections; balanced\n",budgets,failures,publications);
    return 0;
}
