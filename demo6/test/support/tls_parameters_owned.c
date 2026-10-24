/* Configuration, ownership, allocation rollback only; no network or crypto. */
#include "src/runtime/xl6_tls_parameters_internal.h"
#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
static size_t budgets,failures,publications;
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
static xvalue* array(const int64_t* values,size_t count)
{xvalue* a=xrtValueArray();assert(a);for(size_t i=0;i<count;i++)assert(xrtValueArrayAppendNew(a,xrtValueInt(values[i])));return a;}
static void publication(void* (*create)(void),const XL6_NativeHandlePolicyV1* policy,const XL6_OwnershipDomainV1* domain)
{
    bool done=false;
    for(size_t i=0;i<32;i++){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* p=create();assert(p && xrtMemDebugFailAfter(i));
        xvalue* value=xl6NativeHandleTakeV1(&p,policy);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();publications++;
        assert(!p && (hit?(!value && xrtGetError()):(value && !xrtGetError())));xrtClearError();
        if(value){
            const XL6_OwnershipDomainV1* domains[]={domain};XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
            xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count=0;
            assert(xrtOwnershipFreezeTryBegin(&freeze) && ref.Ops->Count(ref.Data,&count) && count==1);
            const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
            assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&finish) && adapter && !preparation && !finish);
            int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
            assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);done=true;
        }
        xrtValueRelease(value);balanced(before);if(done)break;
    }assert(done);
}
static void* limits(void){return xl6TlsLimitsCreateV1(262144,262144,262144,1048576,64,64);}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* policy=xl6TlsPolicyDefaultV1();void* budget=limits();assert(policy && budget);
    assert(xl6TlsPolicyVersionAtV1(policy,0)==XTLS_VERSION_13 && xl6TlsPolicyVersionCountV1(policy)==2 &&
        xl6TlsPolicyCipherCountV1(policy)==9 && xl6TlsPolicyGroupCountV1(policy)==4 && xl6TlsPolicySignatureCountV1(policy)==14);
    FAULTS(xl6TlsPolicyDefaultV1(),xl6TlsPolicyDestroyV1);
    FAULTS(limits(),xl6TlsLimitsDestroyV1);
    FAULTS(xl6TlsParametersDefaultV1(),xl6TlsParametersDestroyV1);
    FAULTS(xl6TlsParametersCreateV1(policy,budget),xl6TlsParametersDestroyV1);
    int64_t v[]={XTLS_VERSION_13},c[]={XTLS_CHACHA20_POLY1305_SHA256,XTLS_AES_128_GCM_SHA256},
        g[]={XTLS_GROUP_X25519,XTLS_GROUP_SECP256R1},s[]={XTLS_SIGNATURE_ED25519,XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256};
    xvalue* versions=array(v,1);xvalue* ciphers=array(c,2);xvalue* groups=array(g,2);xvalue* signatures=array(s,2);
    FAULTS(xl6TlsPolicyPreferencesV1(versions,ciphers,groups,signatures,XTLS_KEY_SHARE_PREFER_READY),xl6TlsPolicyDestroyV1);
    void* selected=xl6TlsPolicyPreferencesV1(versions,ciphers,groups,signatures,XTLS_KEY_SHARE_PREFER_READY);assert(selected);
    xrtValueRelease(versions);xrtValueRelease(ciphers);xrtValueRelease(groups);xrtValueRelease(signatures);
    assert(xl6TlsPolicyVersionCountV1(selected)==1 && xl6TlsPolicyVersionAtV1(selected,0)==XTLS_VERSION_13 &&
        xl6TlsPolicyCipherAtV1(selected,0)==XTLS_CHACHA20_POLY1305_SHA256 &&
        xl6TlsPolicyGroupAtV1(selected,1)==XTLS_GROUP_SECP256R1 &&
        xl6TlsPolicySignatureAtV1(selected,0)==XTLS_SIGNATURE_ED25519 && xl6TlsPolicyKeyShareV1(selected)==XTLS_KEY_SHARE_PREFER_READY);
    void* parameters=xl6TlsParametersCreateV1(selected,budget);assert(parameters);
    xl6TlsPolicyDestroyV1(selected);xl6TlsPolicyDestroyV1(policy);xl6TlsLimitsDestroyV1(budget);
    FAULTS(xl6TlsParametersLimitsV1(parameters),xl6TlsLimitsDestroyV1);
    void* copied=xl6TlsParametersLimitsV1(parameters);void* shared=xl6TlsParametersPolicyV1(parameters);assert(copied && shared);
    xl6TlsParametersDestroyV1(parameters);
    assert(xl6TlsLimitsFeedLimitV1(copied)==262144 && xl6TlsPolicyVersionAtV1(shared,0)==XTLS_VERSION_13);
    xl6TlsLimitsDestroyV1(copied);xl6TlsPolicyDestroyV1(shared);
    assert(!xl6TlsLimitsCreateV1(1,262144,262144,1048576,64,64) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6TlsLimitsCreateV1(262144,262144,262144,1048576,INT64_MAX,64) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    publication(limits,xl6TlsLimitsHandlePolicyV1Get(),xl6OwnershipDomainTlsLimitsV1Get());
    publication(xl6TlsPolicyDefaultV1,xl6TlsPolicyHandlePolicyV1Get(),xl6OwnershipDomainTlsPolicyV1Get());
    publication(xl6TlsParametersDefaultV1,xl6TlsParametersHandlePolicyV1Get(),xl6OwnershipDomainTlsParametersV1Get());
    int64_t many[]={1,2,3};xvalue* input=array(many,3);XL6_ArrayInputV1 owned={0};
    assert(!xl6ArrayInputAcquireBoundedV1(input,2,&owned) && !owned.items && !owned.count && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(xl6ArrayInputAcquireV1(input,&owned) && owned.count==3);xl6ArrayInputReleaseV1(&owned);xrtValueRelease(input);
    balanced(before);assert(xrtMemDebugReset());
    printf("TLS parameters owned: %zu allocation budgets; %zu failures; %zu publication budgets; 3 real catalogs; detached preferences; balanced\n",budgets,failures,publications);
    return 0;
}
