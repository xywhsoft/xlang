/* Ordinary fixed-key verification, error contracts and allocation rollback. */
#include "src/runtime/xl6_x509_verify_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "x509_signature_vectors.h"
static size_t budgets,failures;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static xvalue* input(xbytesview raw){xvalue* value=xrtValueBytes(raw);assert(value);return value;}
static void equal(xvalue* value,xbytesview expected)
{xbytesview view;assert(value && xrtValueGetBytes(value,&view) && view.Size==expected.Size && (!view.Size || !memcmp(view.Data,expected.Data,view.Size)));xrtValueRelease(value);}
static void* certificate(xbytesview raw)
{xvalue* value=input(raw);void* result=xl6X509CertificateFromDerV1(value,0);xrtValueRelease(value);assert(result);return result;}
#define STATUS_FAULTS_VALUE(Call,Expected) do { \
    for(size_t prefix=0;prefix<256;++prefix) { \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix)); \
        int64_t result=(Call);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets; \
        if(hit) {++failures;assert(result==XL6_X509_SIGNATURE_ERROR_V1 && xrtErrorIs(xrtGetError(),XERR_MEMORY));} \
        else assert(result==(Expected) && !xrtGetError()); \
        xrtClearError();balanced(before);if(!hit)break;assert(prefix<255); \
    } \
}while(0)
#define STATUS_FAULTS(Call) STATUS_FAULTS_VALUE(Call,XL6_X509_SIGNATURE_VALID_V1)
#define STRICT_FAULTS(Call) do { \
    for(size_t prefix=0;prefix<256;++prefix) { \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix)); \
        Call;bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets; \
        if(hit){++failures;assert(xrtErrorIs(xrtGetError(),XERR_MEMORY));}else assert(!xrtGetError()); \
        xrtClearError();balanced(before);if(!hit)break;assert(prefix<255); \
    } \
}while(0)
static int64_t expected(const SignatureVector* vector)
{
    if(!vector->supported)return XL6_X509_SIGNATURE_UNSUPPORTED_V1;
    #if defined(TEST_MISSING_RSA)
    if(vector==&fixture_0 || vector==&fixture_4)return XL6_X509_SIGNATURE_UNSUPPORTED_V1;
    #endif
    #if defined(TEST_MISSING_ECDSA)
    if(vector==&fixture_1 || vector==&fixture_2)return XL6_X509_SIGNATURE_UNSUPPORTED_V1;
    #endif
    #if defined(TEST_MISSING_ED25519)
    if(vector==&fixture_3)return XL6_X509_SIGNATURE_UNSUPPORTED_V1;
    #endif
    return XL6_X509_SIGNATURE_VALID_V1;
}
static void check(const SignatureVector* vector)
{
    void* cert=certificate(vector->certificate);void* issuer=certificate(vector->issuer);
    xvalue* der=input(vector->crl);void* crl=xl6X509CrlFromDerV1(der,0);xrtValueRelease(der);assert(crl);
    der=input(vector->spki);void* key=xl6X509PublicKeyInfoFromSpkiV1(der,0);xrtValueRelease(der);assert(key);
    equal(xl6X509PublicKeyInfoSpkiV1(key),vector->spki);
    void* extracted=xl6X509CertificateKeyInfoV1(issuer);assert(extracted);
    equal(xl6X509PublicKeyInfoSpkiV1(extracted),vector->spki);xl6X509PublicKeyInfoDestroyV1(extracted);
    der=input(vector->algorithm);void* algorithm=xl6X509AlgorithmFromDerV1(der,0);xrtValueRelease(der);assert(algorithm);
    void* scheme=xl6X509AlgorithmSignatureInfoV1(algorithm);xl6X509AlgorithmDestroyV1(algorithm);assert(scheme);
    xvalue* message=input(vector->message);xvalue* signature=input(vector->signature);
    void* buffer=xl6BufferCreateV1();assert(buffer && xrtBufferAppend(buffer,vector->message));
    int64_t status=expected(vector);
    assert(xl6X509CertificateCheckSignatureV1(cert,issuer)==status && xl6X509CertificateCheckSignatureKeyV1(cert,key)==status);
    assert(xl6X509CrlCheckSignatureV1(crl,issuer)==status && xl6X509CrlCheckSignatureKeyV1(crl,key)==status);
    assert(xl6X509SignatureInfoCheckBytesV1(scheme,message,signature,key,-1)==status);
    assert(xl6X509SignatureInfoCheckBytesV1(scheme,message,signature,key,INT64_MAX)==status);
    assert(xl6X509SignatureInfoCheckTextV1(scheme,(xstrview){(const char*)vector->message.Data,vector->message.Size},signature,key,-1)==status);
    assert(xl6X509SignatureInfoCheckBufferV1(scheme,buffer,signature,key,INT64_MAX)==status && !xrtGetError());
    if(status==XL6_X509_SIGNATURE_VALID_V1) {
        xl6X509CertificateVerifySignatureV1(cert,issuer);xl6X509CertificateVerifySignatureKeyV1(cert,key);
        xl6X509CrlVerifySignatureV1(crl,issuer);xl6X509CrlVerifySignatureKeyV1(crl,key);
        xl6X509SignatureInfoVerifyBytesV1(scheme,message,signature,key,-1);
        xl6X509SignatureInfoVerifyTextV1(scheme,(xstrview){(const char*)vector->message.Data,vector->message.Size},signature,key,-1);
        xl6X509SignatureInfoVerifyBufferV1(scheme,buffer,signature,key,-1);assert(!xrtGetError());
        STATUS_FAULTS(xl6X509CertificateCheckSignatureV1(cert,issuer));STATUS_FAULTS(xl6X509CertificateCheckSignatureKeyV1(cert,key));
        STATUS_FAULTS(xl6X509CrlCheckSignatureV1(crl,issuer));STATUS_FAULTS(xl6X509CrlCheckSignatureKeyV1(crl,key));
        STATUS_FAULTS(xl6X509SignatureInfoCheckBytesV1(scheme,message,signature,key,-1));
        STRICT_FAULTS(xl6X509CertificateVerifySignatureV1(cert,issuer));STRICT_FAULTS(xl6X509CrlVerifySignatureKeyV1(crl,key));
        STRICT_FAULTS(xl6X509SignatureInfoVerifyBufferV1(scheme,buffer,signature,key,-1));
        /* Two ordinary identities, not a modified/malformed signature. */
        void* other=certificate((vector==&fixture_0 || vector==&fixture_4)?fixture_3.issuer:fixture_0.issuer);
        assert(xl6X509CertificateCheckSignatureV1(cert,other)==XL6_X509_SIGNATURE_REJECTED_V1 && !xrtGetError());
        STATUS_FAULTS_VALUE(xl6X509CertificateCheckSignatureV1(cert,other),XL6_X509_SIGNATURE_REJECTED_V1);
        xl6X509CertificateVerifySignatureV1(cert,other);assert(xrtErrorKind(xrtGetError())==XERR_PROTOCOL);xrtClearError();
        xl6X509CertificateDestroyV1(other);
    } else {
        xl6X509CertificateVerifySignatureV1(cert,issuer);assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);xrtClearError();
        xl6X509CrlVerifySignatureKeyV1(crl,key);assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);xrtClearError();
        xl6X509SignatureInfoVerifyBytesV1(scheme,message,signature,key,-1);assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);xrtClearError();
    }
    xl6BufferDestroyV1(buffer);xrtValueRelease(message);xrtValueRelease(signature);xl6X509SignatureInfoDestroyV1(scheme);
    xl6X509PublicKeyInfoDestroyV1(key);xl6X509CertificateDestroyV1(cert);xl6X509CertificateDestroyV1(issuer);xl6X509CrlDestroyV1(crl);
}
static void spkis(void)
{
    for(size_t i=0;i<sizeof(managed_spkis)/sizeof(*managed_spkis);++i) {
        xvalue* encoded=input(*managed_spkis[i]);
        for(size_t prefix=0;prefix<32;++prefix) {
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix));
            void* key=xl6X509PublicKeyInfoFromSpkiV1(encoded,0);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
            assert(hit?(!key && xrtErrorIs(xrtGetError(),XERR_MEMORY)):(key && !xrtGetError()));if(hit)++failures;
            xrtClearError();xl6X509PublicKeyInfoDestroyV1(key);balanced(before);if(!hit)break;assert(prefix<31);
        }
        void* key=xl6X509PublicKeyInfoFromSpkiV1(encoded,0);xrtValueRelease(encoded);assert(key);
        equal(xl6X509PublicKeyInfoSpkiV1(key),*managed_spkis[i]);xl6X509PublicKeyInfoDestroyV1(key);
    }
}
static xerror* envelope(xerror* cause,xerrkind kind)
{xerror* value=xrtErrorWrap(cause,kind,"xrt.x509",X509_ERROR_SIGNATURE,"synthetic signature contract");assert(value);xrtErrorFree(cause);return value;}
static void outcomes(void)
{
    xerror* error=envelope(NULL,XERR_PROTOCOL);xrtSetErrorTake(error);
    assert(xl6X509SignatureFinishV1(false,NULL,false)==XL6_X509_SIGNATURE_REJECTED_V1 && !xrtGetError());
    error=envelope(NULL,XERR_UNSUPPORTED);xrtSetErrorTake(error);
    assert(xl6X509SignatureFinishV1(false,NULL,false)==XL6_X509_SIGNATURE_UNSUPPORTED_V1 && !xrtGetError());
    error=envelope(envelope(NULL,XERR_UNSUPPORTED),XERR_PROTOCOL);xrtSetErrorTake(error);
    assert(xl6X509SignatureFinishV1(false,NULL,false)==XL6_X509_SIGNATURE_UNSUPPORTED_V1 && !xrtGetError());
    const xerrkind kinds[]={XERR_MEMORY,XERR_ARGUMENT,XERR_RANGE,XERR_STATE,XERR_INTERNAL};
    for(size_t i=0;i<sizeof(kinds)/sizeof(*kinds);++i) {
        error=envelope(xrtErrorCreate(kinds[i],"ordinary.other",19,"checked cause"),XERR_PROTOCOL);xrtSetErrorTake(error);
        assert(xl6X509SignatureFinishV1(false,NULL,false)==XL6_X509_SIGNATURE_ERROR_V1 && xrtGetError()==error);
        assert(xrtErrorIs(error,kinds[i]));xrtClearError();
    }
    error=xrtErrorCreate(XERR_PROTOCOL,"unrelated.domain",X509_ERROR_SIGNATURE,"not X.509");xrtSetErrorTake(error);
    assert(xl6X509SignatureFinishV1(false,NULL,false)==XL6_X509_SIGNATURE_ERROR_V1 && xrtGetError()==error);xrtClearError();
    error=envelope(NULL,XERR_PROTOCOL);xrtSetErrorTake(error);
    assert(xl6X509SignatureFinishV1(false,NULL,true)==XL6_X509_SIGNATURE_ERROR_V1 && xrtGetError()==error);xrtClearError();
    assert(xl6X509SignatureFinishV1(false,NULL,false)==XL6_X509_SIGNATURE_ERROR_V1 && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    error=xrtErrorCreate(XERR_STATE,"previous",1,"first failure");assert(error);
    assert(xl6X509SignatureFinishV1(true,error,false)==XL6_X509_SIGNATURE_VALID_V1 && xrtGetError()==error);xrtClearError();
}
int main(void)
{
    assert(!xrtGetError());xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(size_t i=0;i<sizeof(vectors)/sizeof(*vectors);++i)check(vectors[i]);
    spkis();outcomes();
    balanced(before);assert(!xrtGetError());
    printf("X509 verification: six ordinary signature families, %zu allocation budgets, %zu failures; balanced\n",budgets,failures);return 0;
}
