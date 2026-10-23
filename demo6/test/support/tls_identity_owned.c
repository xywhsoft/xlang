/* Normal identity construction/lifetime and routine allocation rollback only. */
#include "src/runtime/xl6_tls_identity_internal.h"
#include "src/runtime/xl6_x509_internal.h"
#include "src/runtime/xl6_crypto_key_spans.h"
#include "src/runtime/xl6_key_curve_codecs.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "x509_identity_vectors.h"
#include "tls_identity_keys.h"
typedef void* (*Factory)(XL6_NativeClassArrayContextV1,const xvalue*,void*,int64_t,int64_t);
static Factory factories[]={xl6TlsIdentityRsaV1,xl6TlsIdentityP256V1,xl6TlsIdentityP384V1,xl6TlsIdentityEd25519V1};
static void (*drops[])(void*)={xl6CryptoRsaPrivateDestroyV1,xl6CryptoP256DestroyV1,xl6CryptoP384DestroyV1,xl6CryptoEd25519DestroyV1};
static const xtlsidentitytype kinds[]={XTLS_IDENTITY_RSA,XTLS_IDENTITY_ECDSA_P256,XTLS_IDENTITY_ECDSA_P384,XTLS_IDENTITY_ED25519};
static size_t budgets,failures,publications,copyBudgets;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static XL6_NativeClassArrayContextV1 context(void)
{return (XL6_NativeClassArrayContextV1){{701,801,3,sizeof(xvalue*),xrtTypeValue()},"handle",6,xl6X509CertificateHandlePolicyV1Get()};}
static xvalue* chain(const xbytesview* certificates,size_t count)
{
    uint64_t tag=901;XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,0,&tag,1};
    xvalue* array=xl6SequenceNominalCreateV1(1,701,801,&schema,false,801);assert(array);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(array,1,701,801,3,sizeof(xvalue*),xrtTypeValue());assert(pin && xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<count;++i){
        void* raw=xl6X509CertificateCreateSpanV1(certificates[i]);assert(raw);
        xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6X509CertificateHandlePolicyV1Get());assert(handle && !raw);
        xvalue* object=xrtValueObject();assert(object);
        assert(xrtValueObjectSetNewPostCommit(object,(xstrview){"handle",6},handle));
        assert(xrtValueObjectSetNewPostCommit(object,(xstrview){"$xlir.class.runtime-type",24},xrtValueInt(901)));
        assert(xl6SequenceAdmitSlotV1(pin,&object) && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&object));xrtValueRelease(object);
    }
    xl6SequenceEndV1(pin);return array;
}
static xvalue* input(xbytesview certificate)
{return chain(&certificate,certificate.Size?1:0);}
static void* key(size_t i)
{
    xbytesview material=tlsIdentityKeys[i];
    if(i==0)return xl6CryptoRsaPrivateFromPkcs1SpanV1(material);
    if(i==1)return xl6CryptoP256CreateSpanV1(material);
    if(i==2)return xl6CryptoP384CreateSpanV1(material);
    return xl6CryptoEd25519CreateSpanV1(material);
}
static bool memoryCause(const xerror* error)
{for(;error;error=xrtErrorCause(error))if(xrtErrorKind(error)==XERR_MEMORY)return true;return false;}
static void check(size_t i,void* identity)
{
    assert(identity && xl6TlsIdentityKindV1(identity)==(int64_t)kinds[i] && xl6TlsIdentityCountV1(identity)==1);
    void* cert=xl6TlsIdentityCertificateV1(identity,0);assert(cert);
    xbytesview actual=((const XL6_X509CertificateV1*)cert)->view.Raw;
    assert(actual.Size==identityVectors[i].der.Size && !memcmp(actual.Data,identityVectors[i].der.Data,actual.Size));
    xl6X509CertificateDestroyV1(cert);
}
static void constructors(size_t i,xvalue* certificates,void* privateKey)
{
    bool done=false;
    for(size_t budget=0;budget<256;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(budget));
        void* identity=factories[i](context(),certificates,privateKey,64,16777216);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
        assert(hit?(!identity && memoryCause(xrtGetError())):(identity && !xrtGetError()));
        if(hit)++failures;else{done=true;check(i,identity);}xrtClearError();xl6TlsIdentityDestroyV1(identity);balanced(before);if(done)break;
    }
    assert(done);
}
static void publication(size_t i,xvalue* certificates,void* privateKey)
{
    bool done=false;
    for(size_t budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        void* identity=factories[i](context(),certificates,privateKey,64,16777216);assert(identity);
        assert(xrtMemDebugFailAfter(budget));xvalue* value=xl6NativeHandleTakeV1(&identity,xl6TlsIdentityHandlePolicyV1Get());
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++publications;
        assert(!identity && (hit?(!value && xrtGetError()):(value && !xrtGetError())));xrtClearError();
        if(value){
            const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainTlsIdentityV1Get()};
            XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
            xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count=0;
            assert(xrtOwnershipFreezeTryBegin(&freeze) && ref.Ops->Count(ref.Data,&count) && count==1);
            const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
            assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&finish) && adapter && !preparation && !finish);
            int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
            assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);done=true;
        }
        xrtValueRelease(value);balanced(before);if(done)break;
    }
    assert(done);
}
static void certificateCopy(void* identity)
{
    bool done=false;
    for(size_t budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(budget));
        void* cert=xl6TlsIdentityCertificateV1(identity,0);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++copyBudgets;
        assert(hit?(!cert && xrtGetError()):(cert && !xrtGetError()));if(!hit)done=true;
        xrtClearError();xl6X509CertificateDestroyV1(cert);balanced(before);if(done)break;
    }assert(done);
}
static void longChain(void)
{
    xvalue* certificates=chain(tlsIdentityLongChain,9);void* privateKey=key(1);assert(privateKey);
    size_t bytes=0;for(size_t i=0;i<9;++i)bytes+=tlsIdentityLongChain[i].Size;
    assert(!xl6TlsIdentityP256V1(context(),certificates,privateKey,8,0) && xrtGetError());xrtClearError();
    assert(!xl6TlsIdentityP256V1(context(),certificates,privateKey,9,(int64_t)bytes-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    bool done=false;
    for(size_t budget=0;budget<256;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(budget));
        void* identity=xl6TlsIdentityP256V1(context(),certificates,privateKey,0,0);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
        assert(hit?(!identity && memoryCause(xrtGetError())):(identity && !xrtGetError()));
        if(hit)++failures;else{assert(xl6TlsIdentityCountV1(identity)==9);done=true;}
        xrtClearError();xl6TlsIdentityDestroyV1(identity);balanced(before);if(done)break;
    }assert(done);xrtValueRelease(certificates);drops[1](privateKey);
}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(size_t i=0;i<4;++i){
        xvalue* certificates=input(identityVectors[i].der);void* privateKey=key(i);assert(privateKey);
        void* identity=factories[i](context(),certificates,privateKey,1,(int64_t)identityVectors[i].der.Size);check(i,identity);
        constructors(i,certificates,privateKey);publication(i,certificates,privateKey);certificateCopy(identity);
        assert(!factories[i](context(),certificates,privateKey,-1,0) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!factories[i](context(),certificates,privateKey,0,-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!factories[i](context(),certificates,privateKey,1,(int64_t)identityVectors[i].der.Size-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        xvalue* empty=input((xbytesview){0});assert(!factories[i](context(),empty,privateKey,0,0) && xrtGetError());xrtClearError();xrtValueRelease(empty);
        assert(!xl6TlsIdentityCertificateV1(identity,-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!xl6TlsIdentityCertificateV1(identity,1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        xrtSetErrorKind(XERR_CANCELLED);const xerror* prior=xrtGetError();
        assert(!factories[i](context(),certificates,privateKey,0,0) && !xl6TlsIdentityCertificateV1(identity,0) && xrtGetError()==prior);xrtClearError();
        xrtValueRelease(certificates);drops[i](privateKey);check(i,identity);
        static const xtlssignature schemes[]={XTLS_SIGNATURE_RSA_PKCS1_SHA256,XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384,XTLS_SIGNATURE_ED25519};
        uint8 signature[1024];size_t size=0;xbytesview message={(const uint8*)"ordinary offline TLS identity",29};
        xtlsversion version=i?XTLS_VERSION_13:XTLS_VERSION_12;
        assert(xrtTlsIdentityCanSign(identity,version,schemes[i]));
        assert(xrtTlsIdentitySign(identity,version,schemes[i],message,signature,sizeof(signature),&size) && size);
        printf("TLS identity signature %zu ",i);
        for(size_t j=0;j<size;++j)printf("%02x",(unsigned)signature[j]);
        putchar('\n');
        void* cert=xl6TlsIdentityCertificateV1(identity,0);assert(cert);xl6TlsIdentityDestroyV1(identity);
        assert(((const XL6_X509CertificateV1*)cert)->view.Raw.Size==identityVectors[i].der.Size);xl6X509CertificateDestroyV1(cert);
    }
    longChain();assert(!xrtGetError());balanced(before);assert(xrtMemDebugReset());
    printf("TLS identity owned: 4 normal families; %zu factory budgets; %zu failures; %zu publication budgets; %zu copy budgets; detached keys and certificates; catalog balanced\n",budgets,failures,publications,copyBudgets);
    return 0;
}
