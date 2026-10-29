/* Four ordinary, unmodified certificates. No network or crypto security tests. */
#include "src/runtime/xl6_x509_profiles_internal.h"
#include "src/runtime/xl6_x509_identity_abi.h"
#include <assert.h>
#include <stdio.h>
#include "x509_identity_vectors.h"
static size_t budgets,failures,publications;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void copiedName(void* raw)
{
    const XL6_X509GeneralNameV1* name=raw;
    assert(name && !name->constraintBase && name->value.Type==X509_NAME_DNS && name->value.Value.Size==9 && !memcmp(name->value.Value.Data,"localhost",9));
}
static void faultCopy(void* cert)
{
    bool done=false;
    for(size_t budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(budget));
        void* name=xl6X509CertificateMatchHostV1(cert,XRT_STR_LITERAL("localhost"),-1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
        assert(hit?(!name && xrtErrorKind(xrtGetError())==XERR_MEMORY):(name && !xrtGetError()));
        if(hit)++failures;else{done=true;copiedName(name);}xrtClearError();xl6X509GeneralNameDestroyV1(name);balanced(before);if(done)break;
    }assert(done);
}
static void faultPublication(void* cert)
{
    bool done=false;
    for(size_t budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        void* name=xl6X509CertificateMatchHostV1(cert,XRT_STR_LITERAL("localhost"),-1);assert(name);
        assert(xrtMemDebugFailAfter(budget));xvalue* value=xl6NativeHandleTakeV1(&name,xl6X509GeneralNameHandlePolicyV1Get());
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++publications;
        assert(!name && (hit?(!value && xrtGetError()):(value && !xrtGetError())));if(!hit)done=true;
        xrtClearError();xrtValueRelease(value);balanced(before);if(done)break;
    }assert(done);
}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(size_t i=0;i<sizeof(identityVectors)/sizeof(*identityVectors);++i){
        void* cert=xl6X509CertificateCreateSpanV1(identityVectors[i].der);assert(cert);
        assert(xl6X509MatchesDnsV1(XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("LOCALHOST"),-1,-1));
        assert(!xl6X509MatchesDnsV1(XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("other.test"),-1,-1) && !xrtGetError());
        assert(xl6X509CertificateMatchesHostV1(cert,XRT_STR_LITERAL("localhost"),100));
        assert(xl6X509CertificateMatchesHostV1(cert,XRT_STR_LITERAL("127.0.0.1"),-1));
        assert(xl6X509CertificateMatchesHostV1(cert,XRT_STR_LITERAL("::1"),-1));
        assert(!xl6X509CertificateMatchesHostV1(cert,XRT_STR_LITERAL("other.test"),-1) && !xrtGetError());
        assert(!xl6X509CertificateMatchHostV1(cert,XRT_STR_LITERAL("other.test"),-1) && !xrtGetError());
        assert(xrtMemDebugFailAfter(0));bool same=xl6X509CertificateMatchesHostV1(cert,XRT_STR_LITERAL("localhost"),-1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(same && !hit && !xrtGetError());
        assert(!xl6X509CertificateMatchesHostV1(cert,(xstrview){"::1",sizeof("::1")},-1) && xrtErrorKind(xrtGetError())==XERR_VALUE);xrtClearError();
        assert(!xl6X509CertificateMatchesHostV1(cert,XRT_STR_LITERAL("localhost"),-2) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!xl6X509MatchesDnsV1(XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("localhost"),-2,-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        faultCopy(cert);faultPublication(cert);
        void* name=xl6X509CertificateMatchHostV1(cert,XRT_STR_LITERAL("localhost"),-1);assert(name);
        xrtSetErrorKind(XERR_CANCELLED);const xerror* prior=xrtGetError();
        assert(!xl6X509CertificateMatchesHostV1(cert,XRT_STR_LITERAL("localhost"),-1) &&
            !xl6X509CertificateMatchHostV1(cert,XRT_STR_LITERAL("localhost"),-1) && xrtGetError()==prior);xrtClearError();
        xl6X509CertificateDestroyV1(cert);copiedName(name);xl6X509GeneralNameDestroyV1(name);
    }
    assert(!xrtGetError());balanced(before);assert(xrtMemDebugReset());
    printf("X509 identity owned: 4 normal families; %zu copy budgets; %zu failures; %zu publication budgets; detached names; balanced\n",budgets,failures,publications);return 0;
}
