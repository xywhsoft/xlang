/* Offline normal public certificates/CRLs, independent field oracle and
 * owned-data/OOM/leaf lifetime checks. No malformed or security campaigns. */
#include "src/runtime/xl6_x509_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "x509_vectors.h"

static size_t budgets,failures,publications;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void equal(xbytesview actual,xbytesview expected)
{assert(actual.Size==expected.Size && (!actual.Size || !memcmp(actual.Data,expected.Data,actual.Size)));}
static void bytesEqual(xvalue* value,xbytesview expected)
{xbytesview actual;assert(value && xrtValueGetBytes(value,&actual));equal(actual,expected);xrtValueRelease(value);}
static void destroy(bool crl,void* raw)
{if(crl)xl6X509CrlDestroyV1(raw);else xl6X509CertificateDestroyV1(raw);}
static void* construct(const X509Vector* v,xvalue* input,bool pem)
{
    if(v->crl)return pem?xl6X509CrlFromPemV1(v->pem,XL6_X509_TEXT_LIMIT_V1,XL6_X509_DER_LIMIT_V1):xl6X509CrlFromDerV1(input,XL6_X509_DER_LIMIT_V1);
    return pem?xl6X509CertificateFromPemV1(v->pem,XL6_X509_TEXT_LIMIT_V1,XL6_X509_DER_LIMIT_V1):xl6X509CertificateFromDerV1(input,XL6_X509_DER_LIMIT_V1);
}
static void check(const X509Vector* v,void* raw)
{
    assert(raw && !xrtGetError());
    if(v->crl) {
        bytesEqual(xl6X509CrlToDerV1(raw),v->der);bytesEqual(xl6X509CrlTbsV1(raw),v->tbs);
        bytesEqual(xl6X509CrlIssuerV1(raw),v->issuer);bytesEqual(xl6X509CrlSignatureV1(raw),v->signature);
        bytesEqual(xl6X509CrlSignatureOidV1(raw),v->oid);bytesEqual(xl6X509CrlSignatureParametersV1(raw),(xbytesview){0});
        const xx509crl* native=&((XL6_X509CrlV1*)raw)->view;
        bytesEqual(xl6X509CrlRevokedV1(raw),native->Revoked);bytesEqual(xl6X509CrlExtensionsV1(raw),native->Extensions);
        assert(xl6X509CrlVersionV1(raw)==v->version && xl6X509CrlThisUpdateV1(raw)==v->begin);
        assert(xl6X509CrlHasNextUpdateV1(raw)==v->hasEnd);
        if(v->hasEnd)assert(xl6X509CrlNextUpdateV1(raw)==v->end);
        else{assert(!xl6X509CrlNextUpdateV1(raw) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();}
        assert(xl6X509CrlValidAtV1(raw,v->begin));assert(!xl6X509CrlValidAtV1(raw,v->begin-1) && !xrtGetError());
        if(v->hasEnd){assert(xl6X509CrlValidAtV1(raw,v->end));assert(!xl6X509CrlValidAtV1(raw,v->end+1) && !xrtGetError());}
    } else {
        bytesEqual(xl6X509CertificateToDerV1(raw),v->der);bytesEqual(xl6X509CertificateTbsV1(raw),v->tbs);
        bytesEqual(xl6X509CertificateSerialV1(raw),v->serial);bytesEqual(xl6X509CertificateIssuerV1(raw),v->issuer);
        bytesEqual(xl6X509CertificateSubjectV1(raw),v->subject);bytesEqual(xl6X509CertificateSpkiV1(raw),v->spki);
        bytesEqual(xl6X509CertificateSignatureV1(raw),v->signature);bytesEqual(xl6X509CertificateSignatureOidV1(raw),v->oid);
        bytesEqual(xl6X509CertificateSignatureParametersV1(raw),(xbytesview){0});
        bytesEqual(xl6X509CertificateExtensionsV1(raw),((XL6_X509CertificateV1*)raw)->view.Extensions);
        assert(xl6X509CertificateVersionV1(raw)==v->version && xl6X509CertificateNotBeforeV1(raw)==v->begin && xl6X509CertificateNotAfterV1(raw)==v->end);
        assert(xl6X509CertificateValidAtV1(raw,v->begin) && xl6X509CertificateValidAtV1(raw,v->end));
        assert(!xl6X509CertificateValidAtV1(raw,v->begin-1) && !xl6X509CertificateValidAtV1(raw,v->end+1) && !xrtGetError());
    }
    xstrview text=v->crl?xl6X509CrlToPemV1(raw,XL6_X509_TEXT_LIMIT_V1):xl6X509CertificateToPemV1(raw,XL6_X509_TEXT_LIMIT_V1);
    equal((xbytesview){(const uint8*)text.Data,text.Size},(xbytesview){(const uint8*)v->pem.Data,v->pem.Size});
    assert(text.Data[text.Size]==0);xl6X509FreeV1((void*)text.Data);
}
typedef xvalue* (*ByteGetter)(void*);
static const ByteGetter certificateGetters[]={xl6X509CertificateToDerV1,xl6X509CertificateSerialV1,xl6X509CertificateTbsV1,
    xl6X509CertificateIssuerV1,xl6X509CertificateSubjectV1,xl6X509CertificateSpkiV1,xl6X509CertificateExtensionsV1,
    xl6X509CertificateSignatureV1,xl6X509CertificateSignatureOidV1,xl6X509CertificateSignatureParametersV1};
static const ByteGetter crlGetters[]={xl6X509CrlToDerV1,xl6X509CrlTbsV1,xl6X509CrlIssuerV1,xl6X509CrlRevokedV1,
    xl6X509CrlExtensionsV1,xl6X509CrlSignatureV1,xl6X509CrlSignatureOidV1,xl6X509CrlSignatureParametersV1};
static void getterFaults(bool crl,void* raw)
{
    const ByteGetter* getters=crl?crlGetters:certificateGetters;
    size_t count=crl?sizeof(crlGetters)/sizeof(*crlGetters):sizeof(certificateGetters)/sizeof(*certificateGetters);
    for(size_t i=0;i<=count;++i)for(size_t prefix=0;prefix<32;++prefix) {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix));
        xvalue* value=NULL;xstrview text={0};
        if(i<count)value=getters[i](raw);
        else text=crl?xl6X509CrlToPemV1(raw,XL6_X509_TEXT_LIMIT_V1):xl6X509CertificateToPemV1(raw,XL6_X509_TEXT_LIMIT_V1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
        assert(hit?(!value && !text.Data && xrtGetError()):((value || text.Data) && !xrtGetError()));
        if(hit)++failures;
        xrtClearError();xrtValueRelease(value);xl6X509FreeV1((void*)text.Data);balanced(before);if(!hit)break;
        assert(prefix<31);
    }
}
static void constructorFaults(const X509Vector* v,xvalue* input,bool pem)
{
    for(size_t prefix=0;prefix<32;++prefix) {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix));
        void* raw=construct(v,input,pem);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
        assert(hit?(!raw && xrtGetError()):(raw && !xrtGetError()));if(hit)++failures;
        xrtClearError();if(raw)check(v,raw);destroy(v->crl,raw);balanced(before);if(!hit)break;assert(prefix<31);
    }
}
static void publish(const X509Vector* v,xvalue* input)
{
    for(size_t prefix=0;prefix<32;++prefix) {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* raw=construct(v,input,false);assert(raw);
        const XL6_NativeHandlePolicyV1* policy=v->crl?xl6X509CrlHandlePolicyV1Get():xl6X509CertificateHandlePolicyV1Get();
        assert(xrtMemDebugFailAfter(prefix));xvalue* value=xl6NativeHandleTakeV1(&raw,policy);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++publications;
        assert(!raw && (hit?(!value && xrtGetError()):(value && !xrtGetError())));xrtClearError();
        if(value) {
            const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainX509CertificateV1Get(),xl6OwnershipDomainX509CrlV1Get()};
            XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
            xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count=0;
            assert(xrtOwnershipFreezeTryBegin(&freeze) && ref.Ops->Count(ref.Data,&count) && count==1);
            const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
            assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&finish) && adapter && !preparation && !finish);
            int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
            assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);
        }
        xrtValueRelease(value);assert(!xrtGetError());balanced(before);if(!hit)break;assert(prefix<31);
    }
}
static void limits(const X509Vector* v,xvalue* input)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(unsigned mode=0;mode<2;++mode) {
        int64_t der=mode?(int64_t)v->der.Size:0,text=mode?(int64_t)v->pem.Size:0;
        void* raw=v->crl?xl6X509CrlFromDerV1(input,der):xl6X509CertificateFromDerV1(input,der);
        check(v,raw);destroy(v->crl,raw);
        raw=v->crl?xl6X509CrlFromPemV1(v->pem,text,der):xl6X509CertificateFromPemV1(v->pem,text,der);
        check(v,raw);
        xstrview encoded=v->crl?xl6X509CrlToPemV1(raw,text):xl6X509CertificateToPemV1(raw,text);assert(encoded.Data);
        xl6X509FreeV1((void*)encoded.Data);
        encoded=v->crl?xl6X509CrlToPemV1(raw,(int64_t)v->pem.Size-1):xl6X509CertificateToPemV1(raw,(int64_t)v->pem.Size-1);
        assert(!encoded.Data && !encoded.Size && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();destroy(v->crl,raw);
    }
    for(unsigned mode=0;mode<2;++mode) {
        int64_t der=mode?-1:(int64_t)v->der.Size-1,text=mode?-1:(int64_t)v->pem.Size-1;
        xerrkind expected=mode?XERR_ARGUMENT:XERR_RANGE;
        void* raw=v->crl?xl6X509CrlFromDerV1(input,der):xl6X509CertificateFromDerV1(input,der);
        assert(!raw && xrtErrorKind(xrtGetError())==expected);xrtClearError();
        raw=v->crl?xl6X509CrlFromPemV1(v->pem,text,0):xl6X509CertificateFromPemV1(v->pem,text,0);
        assert(!raw && xrtErrorKind(xrtGetError())==expected);xrtClearError();
        raw=v->crl?xl6X509CrlFromPemV1(v->pem,0,der):xl6X509CertificateFromPemV1(v->pem,0,der);
        assert(!raw && xrtErrorKind(xrtGetError())==expected);xrtClearError();
    }
    balanced(before);
}
int main(void)
{
    assert(!xrtGetError());xmemdebugsnapshot start;xrtMemDebugSnapshot(&start);
    for(size_t i=0;i<sizeof(x509Vectors)/sizeof(*x509Vectors);++i) {
        const X509Vector* v=&x509Vectors[i];xvalue* input=xrtValueBytes(v->der);assert(input);
        for(unsigned pem=0;pem<2;++pem){void* raw=construct(v,input,pem!=0);check(v,raw);getterFaults(v->crl,raw);destroy(v->crl,raw);constructorFaults(v,input,pem!=0);}
        bytes copy=xrtMalloc(v->der.Size);assert(copy);memcpy(copy,v->der.Data,v->der.Size);
        void* independent=v->crl?xl6X509CrlCreateSpanV1((xbytesview){copy,v->der.Size}):xl6X509CertificateCreateSpanV1((xbytesview){copy,v->der.Size});
        memset(copy,0,v->der.Size);xrtFree(copy);check(v,independent);
        xerror* prior=xrtErrorCreate(XERR_STATE,"test",1,"first diagnostic");assert(prior);xrtSetErrorTake(prior);
        destroy(v->crl,independent);assert(xrtGetError()==prior);xrtClearError();
        publish(v,input);limits(v,input);xrtValueRelease(input);balanced(start);
    }
    printf("X509 owned: %zu valid vectors, %zu allocation budgets, %zu failures, %zu publication budgets; balanced\n",
        sizeof(x509Vectors)/sizeof(*x509Vectors),budgets,failures,publications);return 0;
}
