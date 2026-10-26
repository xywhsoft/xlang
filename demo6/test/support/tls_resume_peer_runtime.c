/* Ordinary sealed class-array import ownership and allocation rollback only. */
#include "src/runtime/xl6_tls_internal.h"
#include "src/runtime/xl6_x509_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include <assert.h>
#include <stdio.h>
#include "tls_verifier_vectors.h"

static xvalue* certificates(xbytesview der,size_t count)
{
    void* raw=xl6X509CertificateCreateSpanV1(der);assert(raw);
    xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6X509CertificateHandlePolicyV1Get());assert(handle&&!raw);
    xvalue* object=xrtValueObject();xvalue* tag=xrtValueInt(101);assert(object&&tag);
    assert(xrtValueObjectSet(object,(xstrview){"$xlir.class.runtime-type",24},tag));
    assert(xrtValueObjectSet(object,(xstrview){"handle",6},handle));
    xrtValueRelease(tag);xrtValueRelease(handle);
    uint64_t classTag=101;XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,0,&classTag,1};
    xvalue* array=xl6SequenceNominalCreateV1(1,201,202,&schema,false,202);assert(array);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(array,1,201,202,3,sizeof(xvalue*),xrtTypeValue());
    assert(pin&&xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<count;i++)assert(xl6SequenceAdmitSlotV1(pin,&object)&&xrtTypedArrayPush(xl6SequenceStorageV1(pin),&object));
    xl6SequenceEndV1(pin);xrtValueRelease(object);return array;
}

static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
}

static void exercise(size_t vector,size_t count,size_t prefix)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xvalue* array=certificates(pathVectors[vector].leaf,count);
    xtlsresumeconfig config;xrtTlsResumeConfigInit(&config);
    config.Cipher=XTLS_AES_128_GCM_SHA256;config.Ticket=XRT_BYTES_LITERAL("ordinary-ticket");
    config.Secret=XRT_BYTES_LITERAL("01234567890123456789012345678901");config.Lifetime=120;
    xtlsresume* raw=xrtTlsResumeCreate(&config);assert(raw);
    XL6_NativeClassArrayContextV1 context={{201,202,3,sizeof(xvalue*),xrtTypeValue()},
        "handle",6,xl6X509CertificateHandlePolicyV1Get()};
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
    if(prefix!=SIZE_MAX)assert(xrtMemDebugFailAfter(prefix));
    void* copy=xl6TlsResumeWithPeerCertificatesV1(context,raw,array,64,16777216);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    assert(hit?(!copy&&!outcome.ok):(copy&&outcome.ok));
    xrtErrorFree(outcome.error);
    assert(xl6TlsResumePeerCertificateCountV1(raw)==0);
    xrtValueRelease(array);xrtTlsResumeRelease(raw);
    if(copy){
        assert(xl6TlsResumePeerCertificateCountV1(copy)==(int64_t)count);
        void* detached=xl6TlsResumePeerCertificateV1(copy,(int64_t)count-1);assert(detached);
        xl6TlsResumeDestroyV1(copy);
        const xx509cert* parsed=&((XL6_X509CertificateV1*)detached)->view;
        assert(parsed->Raw.Size==pathVectors[vector].leaf.Size&&
            !memcmp(parsed->Raw.Data,pathVectors[vector].leaf.Data,parsed->Raw.Size));
        xl6X509CertificateDestroyV1(detached);
    }
    assert(!xrtGetError());balanced(before);
}

int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));
    xl6_error_result ready=xl6ErrorScopeEnd(&warm);assert(ready.ok);xrtErrorFree(ready.error);
    for(size_t vector=0;vector<4;vector++)exercise(vector,2,SIZE_MAX);
    for(size_t n=0;n<2;n++)for(size_t prefix=0;prefix<128;prefix++)exercise(3,n?9:2,prefix);
    puts("TLS resume peer runtime: four families, short/heap view arrays, detached outputs, 256 capture/import OOM prefixes; memory balanced");
    return 0;
}
