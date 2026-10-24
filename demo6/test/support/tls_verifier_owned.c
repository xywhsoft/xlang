/* Fixed, ordinary certificates and checked resource/lifetime boundaries only.
 * No network, malformed crypto, original native security tests or trust bypass. */
#include "src/runtime/xl6_tls_verifier_internal.h"
#include "src/runtime/xl6_x509_store_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "tls_verifier_vectors.h"
static size_t budgets,failures,publications,verifications;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
#define FAULTS(Call,Drop) do{for(size_t i=0;i<256;i++){ \
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(i)); \
    void* p=(Call);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();budgets++; \
    assert(hit?(!p && xrtGetError()):(p && !xrtGetError()));if(hit)failures++; \
    xrtClearError();Drop(p);balanced(before);if(!hit)break;assert(i<255);}}while(0)
static XL6_NativeClassArrayContextV1 context(void)
{return (XL6_NativeClassArrayContextV1){{701,801,3,sizeof(xvalue*),xrtTypeValue()},"handle",6,xl6X509CertificateHandlePolicyV1Get()};}
static xvalue* certificates(const xbytesview* certificates,size_t count)
{
    uint64_t tag=901;XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,0,&tag,1};
    xvalue* array=xl6SequenceNominalCreateV1(1,701,801,&schema,false,801);assert(array);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(array,1,701,801,3,sizeof(xvalue*),xrtTypeValue());assert(pin && xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<count;i++){
        void* raw=xl6X509CertificateCreateSpanV1(certificates[i]);assert(raw);
        xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6X509CertificateHandlePolicyV1Get());assert(handle && !raw);
        xvalue* object=xrtValueObject();assert(object);
        assert(xrtValueObjectSetNewPostCommit(object,(xstrview){"handle",6},handle));
        assert(xrtValueObjectSetNewPostCommit(object,(xstrview){"$xlir.class.runtime-type",24},xrtValueInt(901)));
        assert(xl6SequenceAdmitSlotV1(pin,&object) && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&object));xrtValueRelease(object);
    }xl6SequenceEndV1(pin);return array;
}
static void* trust(xbytesview der)
{
    xx509store* store=xrtX509StoreCreate();assert(store && xrtX509StoreAdd(store,der.Data,der.Size)==X509_VALUE);
    void* version=xl6X509StoreVersionTakeV1(&store,(int64_t)der.Size);assert(version && !store);return version;
}
static void publication(void* (*make)(void*),void* argument,const XL6_NativeHandlePolicyV1* policy,const XL6_OwnershipDomainV1* domain)
{
    bool done=false;
    for(size_t i=0;i<32;i++){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* raw=make(argument);assert(raw && xrtMemDebugFailAfter(i));
        xvalue* value=xl6NativeHandleTakeV1(&raw,policy);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();publications++;
        assert(!raw && (hit?(!value && xrtGetError()):(value && !xrtGetError())));xrtClearError();
        if(value){
            const XL6_OwnershipDomainV1* domains[]={domain};XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
            xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count=0;
            assert(xrtOwnershipFreezeTryBegin(&freeze) && ref.Ops->Count(ref.Data,&count) && count==1);
            const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* finish=NULL;
            assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&prep,&finish) && adapter && !prep && !finish);
            int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
            assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);done=true;
        }
        xrtValueRelease(value);balanced(before);if(done)break;
    }assert(done);
}
static void verifyFaults(void* verifier,const xvalue* chain)
{
    bool complete=false;
    for(size_t i=0;i<512;i++){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(i));
        xl6TlsVerifierServerV1(context(),verifier,chain,(xstrview){"localhost",9},-1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();verifications++;
        if(hit){assert(xrtGetError());failures++;}else{assert(!xrtGetError());complete=true;}
        xrtClearError();balanced(before);if(complete)break;assert(i<511);
    }assert(complete);
}
static void exercise(xbytesview root,const xbytesview* server,size_t count,const xbytesview* client)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* source=trust(root);void* policy=xl6TlsVerificationPolicyCreateV1(64,16777216,64,100000,4096,67108864,false);assert(policy);
    void* fixed=xl6TlsVerificationPolicyWithTimeV1(policy,pathTime);assert(fixed && xl6TlsVerificationPolicyUsesCurrentTimeV1(policy));
    void* epoch=xl6TlsVerificationPolicyWithTimeV1(policy,0);assert(epoch && !xl6TlsVerificationPolicyUsesCurrentTimeV1(epoch) && xl6TlsVerificationPolicyFixedTimeV1(epoch)==0);xl6TlsVerificationPolicyDestroyV1(epoch);
    void* current=xl6TlsVerificationPolicyWithCurrentTimeV1(fixed);assert(current && xl6TlsVerificationPolicyUsesCurrentTimeV1(current));xl6TlsVerificationPolicyDestroyV1(current);
    xvalue* chain=certificates(server,count);xvalue* clientChain=client?certificates(client,2):NULL;
    FAULTS(xl6TlsVerifierCreatePolicyV1(source,fixed),xl6TlsVerifierDestroyV1);
    void* verifier=xl6TlsVerifierCreatePolicyV1(source,fixed);assert(verifier);
    FAULTS(xl6TlsVerifierPolicyV1(verifier),xl6TlsVerificationPolicyDestroyV1);
    FAULTS(xl6TlsVerifierCopyV1(verifier),xl6TlsVerifierDestroyV1);
    void* copy=xl6TlsVerifierCopyV1(verifier);assert(copy);
    xl6TlsVerifierDestroyV1(verifier);xl6X509StoreSnapshotDestroyV1(source);xl6TlsVerificationPolicyDestroyV1(policy);xl6TlsVerificationPolicyDestroyV1(fixed);
    void* captured=xl6TlsVerifierPolicyV1(copy);assert(captured && xl6TlsVerificationPolicyFixedTimeV1(captured)==pathTime);
    xl6TlsVerificationPolicyDestroyV1(captured);
    xl6TlsVerifierServerV1(context(),copy,chain,(xstrview){"localhost",9},-1);assert(!xrtGetError());
    if(clientChain){xl6TlsVerifierClientV1(context(),copy,clientChain);assert(!xrtGetError());}
    verifyFaults(copy,chain);
    /* Model a future native session retaining only the verifier, not this
     * language wrapper. Its fixed clock must not borrow wrapper storage. */
    xtlsverifier* native=xrtTlsVerifierRetain(((XL6_TlsVerifierV1*)copy)->value);assert(native);xl6TlsVerifierDestroyV1(copy);
    xx509cert* views=xrtCalloc(count,sizeof(*views));assert(views);
    for(size_t i=0;i<count;i++)assert(xrtX509Parse(server[i].Data,server[i].Size,&views[i]));
    assert(xrtTlsVerifierVerify(native,XTLS_SERVER,(xstrview){"localhost",9},views,count));
    xrtFree(views);xrtTlsVerifierRelease(native);xrtValueRelease(chain);xrtValueRelease(clientChain);balanced(before);
}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(size_t i=0;i<4;i++){
        xbytesview server[2],client[2];server[0]=pathVectors[i].leaf;server[1]=pathVectors[i].mid;client[0]=tlsClients[i];client[1]=pathVectors[i].mid;
        exercise(pathVectors[i].root,server,2,client);
    }
    xbytesview deep[9];for(size_t i=0;i<9;i++)deep[i]=tlsDeep[9-i];exercise(tlsDeep[0],deep,9,NULL);
    void* source=trust(pathVectors[1].root);void* policy=xl6TlsVerificationPolicyCreateV1(1,1,1,1,1,1,false);assert(policy);
    assert(!xl6TlsVerifierCreatePolicyV1(source,policy) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();xl6TlsVerificationPolicyDestroyV1(policy);
    publication(xl6TlsVerifierCreateV1,source,xl6TlsVerifierHandlePolicyV1Get(),xl6OwnershipDomainTlsVerifierV1Get());
    void* value=xl6TlsVerifierCreateV1(source);assert(value);
    xbytesview ordinary[2];ordinary[0]=pathVectors[1].leaf;ordinary[1]=pathVectors[1].mid;xvalue* currentChain=certificates(ordinary,2);
    xl6TlsVerifierServerV1(context(),value,currentChain,(xstrview){"localhost",9},-1);assert(!xrtGetError());xrtValueRelease(currentChain);
    publication(xl6TlsVerifierPolicyV1,value,xl6TlsVerificationPolicyHandlePolicyV1Get(),xl6OwnershipDomainTlsVerificationPolicyV1Get());
    xl6TlsVerifierDestroyV1(value);xl6X509StoreSnapshotDestroyV1(source);
    balanced(before);assert(xrtMemDebugReset());
    printf("TLS verifier owned: 4 normal server/client families; 9-certificate path; %zu construction budgets; %zu failures; %zu verification budgets; %zu publication budgets; native-only retain; balanced\n",budgets,failures,verifications,publications);
    return 0;
}
