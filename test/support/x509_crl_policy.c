/* Ordinary fixed-key policy data, ownership and allocation-failure contracts. */
#include "src/runtime/xl6_x509_crl_policy_internal.h"
#include "src/runtime/xl6_x509_crl_critical_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "x509_crl_policy_vectors.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
static size_t budgets,failures,publications;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
#define FAULTS(Call,Drop) do { \
    for(size_t prefix=0;prefix<128;++prefix) { \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix)); \
        void* output=(void*)(Call);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets; \
        assert(hit?(!output && xrtGetError()):(output && !xrtGetError()));if(hit)++failures; \
        xrtClearError();Drop(output);balanced(before);if(!hit)break;assert(prefix<127); \
    } \
}while(0)
static bool publish(void* raw,const XL6_NativeHandlePolicyV1* policy,size_t prefix)
{
    assert(raw && xrtMemDebugFailAfter(prefix));xvalue* value=xl6NativeHandleTakeV1(&raw,policy);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++publications;
    assert(!raw && (hit?(!value && xrtGetError()):(value && !xrtGetError())));xrtClearError();
    xrtValueRelease(value);assert(!xrtGetError());return hit;
}
static void catalog(void* raw,const XL6_NativeHandlePolicyV1* policy,const XL6_OwnershipDomainV1* domain)
{
    xvalue* value=xl6NativeHandleTakeV1(&raw,policy);assert(value && !raw);
    XL6_OwnershipCatalog* registry=xl6OwnershipCatalogCreateV1(&domain,1);assert(registry);
    xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count=0;
    assert(xrtOwnershipFreezeTryBegin(&freeze) && ref.Ops->Count(ref.Data,&count) && count==1);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    assert(xl6OwnershipCatalogResolveV1(ref,registry,&adapter,&preparation,&finish) && adapter && !preparation && !finish);
    int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(registry);
    xrtValueRelease(value);assert(!xrtGetError());
}
#define CATALOG(Call,Part) catalog(Call,xl6X509##Part##HandlePolicyV1Get(),xl6OwnershipDomainX509##Part##V1Get())
#define PUBLISH(Call,Part) do { \
    for(size_t prefix=0;prefix<32;++prefix) { \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before); \
        bool hit=publish(Call,xl6X509##Part##HandlePolicyV1Get(),prefix); \
        balanced(before);if(!hit)break;assert(prefix<31); \
    } \
}while(0)
static void state(void* result,int64_t expected,uint64_t coverage)
{assert(result && xl6X509RevocationStatusV1(result)==expected && xl6X509RevocationCoverageV1(result)==coverage && !xrtGetError());}
static void* validated(xbytesview data,void* issuer,const xx509crlconfig* config)
{
    void* crl=xl6X509CrlCreateSpanV1(data);assert(crl);
    FAULTS(xl6X509ValidatedCrlCreateV1(crl,issuer,config),xl6X509ValidatedCrlDestroyV1);
    void* result=xl6X509ValidatedCrlCreateV1(crl,issuer,config);assert(result);
    xl6X509CrlDestroyV1(crl);return result;
}
static void* queried(void* source,void* target,bool set)
{void* result=set?xl6X509CrlSetQueryV1(source,target):xl6X509ValidatedCrlQueryV1(source,target);assert(result);return result;}
static void* next(void* previous,void* source,bool set)
{void* result=set?xl6X509RevocationCheckWithSetV1(previous,source):xl6X509RevocationCheckWithCrlV1(previous,source);assert(result);return result;}
static xx509result critical(const xx509crl* crl,const xx509crlentry* entry,const xx509ext* extension,void* data)
{
    assert(crl && extension && extension->Critical && extension->Value.Size==5);
    assert(extension->Oid.Size==4 && extension->Oid.Data[0]==42);
    unsigned* seen=data;*seen|=entry?2u:1u;return X509_VALUE;
}
static xx509result resourceFailure(const xx509crl* crl,const xx509crlentry* entry,const xx509ext* extension,void* data)
{(void)crl;(void)entry;(void)extension;(void)data;xrtSetErrorKind(XERR_MEMORY);return X509_ERROR;}
typedef struct Reads {void* crl;void* target;} Reads;
static void readMany(Reads* input)
{
    for(unsigned i=0;i<32;++i) {
        void* owner=xl6X509ValidatedCrlRefV1(input->crl);assert(owner);
        void* result=queried(owner,input->target,false);
        state(result,XL6_X509_REVOCATION_REVOKED_V1,X509_CRL_REASON_FLAG_ALL);
        xl6X509RevocationDestroyV1(result);xl6X509ValidatedCrlDestroyV1(owner);
    }
}
#if defined(_WIN32)
static DWORD WINAPI worker(void* data){readMany(data);return 0;}
#else
static void* worker(void* data){readMany(data);return NULL;}
#endif
static void concurrent(void* crl,void* target)
{
    Reads data={crl,target};
#if defined(_WIN32)
    HANDLE workers[4];
    for(unsigned i=0;i<4;++i){workers[i]=CreateThread(NULL,0,worker,&data,0,NULL);assert(workers[i]);}
    for(unsigned i=0;i<4;++i){assert(WaitForSingleObject(workers[i],INFINITE)==WAIT_OBJECT_0);assert(CloseHandle(workers[i]));}
#else
    pthread_t workers[4];
    for(unsigned i=0;i<4;++i)assert(!pthread_create(&workers[i],NULL,worker,&data));
    for(unsigned i=0;i<4;++i)assert(!pthread_join(workers[i],NULL));
#endif
}
static void ordinary(void)
{
    xx509crlconfig config;assert(xl6X509CrlPolicyConfigV1(policy_time,0,XL6_X509_CRL_STRICT_FLAGS_V1,NULL,NULL,&config));
    void* issuer=xl6X509CertificateCreateSpanV1(policy_issuer_view);
    void* target=xl6X509CertificateCreateSpanV1(policy_target_view);
    void* clear=xl6X509CertificateCreateSpanV1(policy_clear_view);
    assert(issuer && target && clear);
    void* base=validated(policy_base_view,issuer,&config);void* delta=validated(policy_delta_view,issuer,&config);
    void* left=validated(policy_left_view,issuer,&config);void* right=validated(policy_right_view,issuer,&config);
    void* full=validated(policy_complete_view,issuer,&config);void* indirect=validated(policy_indirect_view,issuer,&config);
    ++config.Time;void* laterDelta=validated(policy_delta_view,issuer,&config);void* laterRight=validated(policy_right_view,issuer,&config);--config.Time;
    xl6X509CertificateDestroyV1(issuer);
    assert(!xl6X509CrlSetCreateV1(base,laterDelta) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    concurrent(base,target);
    FAULTS(xl6X509CrlSetCreateV1(base,delta),xl6X509CrlSetDestroyV1);
    void* set=xl6X509CrlSetCreateV1(base,delta);assert(set);
    PUBLISH(xl6X509ValidatedCrlRefV1(base),ValidatedCrl);
    PUBLISH(xl6X509CrlSetCreateV1(base,delta),CrlSet);
    PUBLISH(xl6X509ValidatedCrlQueryV1(base,target),Revocation);
    PUBLISH(xl6X509RevocationCheckCreateV1(clear),RevocationCheck);
    CATALOG(xl6X509ValidatedCrlRefV1(base),ValidatedCrl);
    CATALOG(xl6X509CrlSetCreateV1(base,delta),CrlSet);
    CATALOG(xl6X509ValidatedCrlQueryV1(base,target),Revocation);
    CATALOG(xl6X509RevocationCheckCreateV1(clear),RevocationCheck);
    CATALOG(xl6X509CrlCriticalCreateV1(),CrlCritical);
    PUBLISH(xl6X509CrlCriticalCreateV1(),CrlCritical);
    FAULTS(xl6X509ValidatedCrlQueryV1(base,target),xl6X509RevocationDestroyV1);
    FAULTS(xl6X509CrlSetQueryV1(set,target),xl6X509RevocationDestroyV1);
    void* result=queried(base,target,false);state(result,XL6_X509_REVOCATION_REVOKED_V1,X509_CRL_REASON_FLAG_ALL);
    void* entry=xl6X509RevocationEntryV1(result);assert(entry);
    xl6X509RevocationDestroyV1(result);
    assert(xl6X509CrlEntryReasonV1(entry)==X509_CRL_REASON_CERTIFICATE_HOLD);
    assert(xl6X509CrlEntryHasInvalidityDateV1(entry));xl6X509CrlEntryDestroyV1(entry);
    result=queried(delta,target,false);state(result,XL6_X509_REVOCATION_REMOVED_V1,X509_CRL_REASON_FLAG_ALL);xl6X509RevocationDestroyV1(result);
    result=queried(delta,clear,false);state(result,XL6_X509_REVOCATION_UNKNOWN_V1,0);assert(!xl6X509RevocationObservedV1(result));xl6X509RevocationDestroyV1(result);
    result=queried(set,target,true);state(result,XL6_X509_REVOCATION_GOOD_V1,X509_CRL_REASON_FLAG_ALL);
    entry=xl6X509RevocationEntryV1(result);assert(entry && xl6X509CrlEntryReasonV1(entry)==X509_CRL_REASON_REMOVE);
    xl6X509CrlEntryDestroyV1(entry);xl6X509RevocationDestroyV1(result);
    result=queried(left,clear,false);state(result,XL6_X509_REVOCATION_UNKNOWN_V1,X509_CRL_REASON_FLAG_KEY_COMPROMISE);
    assert(xl6X509RevocationObservedV1(result));xl6X509RevocationDestroyV1(result);
    void* unrelated=xl6X509CertificateCreateSpanV1(policy_unrelated_view);assert(unrelated);
    result=queried(full,unrelated,false);state(result,XL6_X509_REVOCATION_UNKNOWN_V1,0);xl6X509RevocationDestroyV1(result);xl6X509CertificateDestroyV1(unrelated);
    void* indirectTarget=xl6X509CertificateCreateSpanV1(policy_indirect_target_view);assert(indirectTarget);
    result=queried(indirect,indirectTarget,false);state(result,XL6_X509_REVOCATION_REVOKED_V1,X509_CRL_REASON_FLAG_ALL);
    xl6X509RevocationDestroyV1(result);xl6X509CertificateDestroyV1(indirectTarget);

    FAULTS(xl6X509RevocationCheckCreateV1(clear),xl6X509RevocationCheckDestroyV1);
    void* empty=xl6X509RevocationCheckCreateV1(clear);assert(empty);
    xl6X509CertificateDestroyV1(clear);clear=NULL;
    FAULTS(xl6X509RevocationCheckResultV1(empty),xl6X509RevocationDestroyV1);
    FAULTS(xl6X509RevocationCheckWithCrlV1(empty,left),xl6X509RevocationCheckDestroyV1);
    void* partial=next(empty,left,false);assert(!xl6X509RevocationCheckDeterminedV1(partial));
    assert(!xl6X509RevocationCheckWithCrlV1(partial,laterRight) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    result=xl6X509RevocationCheckResultV1(partial);state(result,XL6_X509_REVOCATION_UNKNOWN_V1,X509_CRL_REASON_FLAG_KEY_COMPROMISE);
    assert(xl6X509RevocationObservedV1(result));xl6X509RevocationDestroyV1(result);
    FAULTS(xl6X509RevocationCheckWithCrlV1(partial,right),xl6X509RevocationCheckDestroyV1);
    void* complete=next(partial,right,false);assert(xl6X509RevocationCheckDeterminedV1(complete));
    result=xl6X509RevocationCheckResultV1(complete);state(result,XL6_X509_REVOCATION_GOOD_V1,X509_CRL_REASON_FLAG_ALL);
    assert(!xl6X509RevocationCheckDeterminedV1(partial) && !xl6X509RevocationCheckDeterminedV1(empty));
    assert(!xl6X509RevocationCheckWithCrlV1(complete,base) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xl6X509RevocationCheckDestroyV1(complete);state(result,XL6_X509_REVOCATION_GOOD_V1,X509_CRL_REASON_FLAG_ALL);xl6X509RevocationDestroyV1(result);
    xl6X509RevocationCheckDestroyV1(partial);xl6X509RevocationCheckDestroyV1(empty);

    empty=xl6X509RevocationCheckCreateV1(target);assert(empty);
    assert(!xl6X509RevocationCheckWithCrlV1(empty,delta) && xrtGetError());xrtClearError();assert(!xl6X509RevocationCheckDeterminedV1(empty));
    FAULTS(xl6X509RevocationCheckWithCrlV1(empty,base),xl6X509RevocationCheckDestroyV1);
    FAULTS(xl6X509RevocationCheckWithSetV1(empty,set),xl6X509RevocationCheckDestroyV1);
    complete=next(empty,base,false);xl6X509RevocationCheckDestroyV1(empty);
    result=xl6X509RevocationCheckResultV1(complete);xl6X509RevocationCheckDestroyV1(complete);
    state(result,XL6_X509_REVOCATION_REVOKED_V1,X509_CRL_REASON_FLAG_ALL);
    xl6X509ValidatedCrlDestroyV1(base);xl6X509ValidatedCrlDestroyV1(delta);
    xl6X509ValidatedCrlDestroyV1(left);xl6X509ValidatedCrlDestroyV1(right);xl6X509ValidatedCrlDestroyV1(full);xl6X509ValidatedCrlDestroyV1(indirect);
    xl6X509ValidatedCrlDestroyV1(laterDelta);xl6X509ValidatedCrlDestroyV1(laterRight);
    /* The set keeps its two owners alive; the result owns its entry separately. */
    void* again=queried(set,target,true);xl6X509RevocationDestroyV1(again);xl6X509CrlSetDestroyV1(set);
    entry=xl6X509RevocationEntryV1(result);assert(entry);xl6X509RevocationDestroyV1(result);
    assert(xl6X509CrlEntryReasonV1(entry)==X509_CRL_REASON_CERTIFICATE_HOLD);xl6X509CrlEntryDestroyV1(entry);
    xl6X509CertificateDestroyV1(target);xl6X509CertificateDestroyV1(clear);
}
static void callbacks(void)
{
    void* issuer=xl6X509CertificateCreateSpanV1(policy_issuer_view);unsigned seen=0;
    xx509crlconfig config;assert(xl6X509CrlPolicyConfigV1(policy_time,0,31,critical,&seen,&config));
    void* value=validated(policy_critical_view,issuer,&config);assert(seen==3);
    xl6X509ValidatedCrlDestroyV1(value);
    config.Critical=resourceFailure;config.UserData=NULL;
    void* crl=xl6X509CrlCreateSpanV1(policy_critical_view);assert(crl);
    assert(!xl6X509ValidatedCrlCreateV1(crl,issuer,&config) && xrtErrorIs(xrtGetError(),XERR_MEMORY));xrtClearError();
    xl6X509CrlDestroyV1(crl);xl6X509CertificateDestroyV1(issuer);
    config=(xx509crlconfig){.Time=123};xx509crlconfig before=config;
    assert(!xl6X509CrlPolicyConfigV1(0,-1,31,NULL,NULL,&config) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!memcmp(&before,&config,sizeof(config)));
    assert(!xl6X509CrlPolicyConfigV1(0,0,UINT64_MAX,NULL,NULL,&config) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    assert(!xl6X509CrlPolicyConfigV1(0,0,X509_CRL_REQUIRE_KEY_IDENTIFIER,NULL,NULL,&config) && xrtGetError());xrtClearError();
    xatomic64 count;xrtAtomic64Init(&count,UINT64_MAX);
    assert(!xl6X509PolicyRetainV1(&count) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    xx509revocation invalid={.State=(xx509revocationstate)99,.CoveredReasons=X509_CRL_REASON_FLAG_ALL};
    assert(!xl6X509RevocationCopyV1(&invalid,X509_CRL_VERSION_2,true) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    invalid=(xx509revocation){.State=X509_REVOCATION_REVOKED,.CoveredReasons=X509_CRL_REASON_FLAG_ALL};
    assert(!xl6X509RevocationCopyV1(&invalid,X509_CRL_VERSION_2,true) && xrtGetError());xrtClearError();
}
typedef struct LanguageCall {
    void* context;
    void* crl;
    void* issuer;
    void* savedEntry;
    void* savedExtension;
    unsigned calls;
    bool reject,fail,noDiagnostic,nested,scopeChecked;
} LanguageCall;
static void borrowedEnvironmentDrop(void* raw){(void)raw;}
static bool borrowedEnvironmentTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{(void)raw;(void)visit;(void)user;return true;}
static bool language(void* raw,bool* output)
{
    LanguageCall* call=raw;++call->calls;
    void* entry=xl6X509CrlCriticalEntryV1(call->context);
    if(xrtGetError())return false;
    void* extension=xl6X509CrlCriticalExtensionV1(call->context);
    if(!extension){xl6X509CrlEntryDestroyV1(entry);return false;}
    xl6X509CrlEntryDestroyV1(call->savedEntry);xl6X509ExtensionDestroyV1(call->savedExtension);
    call->savedEntry=entry;call->savedExtension=extension;
    if(call->nested) {
        call->scopeChecked=true;
        XL6_Closure* callback=xl6ClosureRuntimeV1Get()->Create(NULL,(void (*)(void))language,NULL,call,borrowedEnvironmentDrop,NULL,NULL,0,borrowedEnvironmentTrace);assert(callback);
        assert(!xl6X509CrlValidateCallbackV1(call->crl,call->issuer,policy_time,0,31,call->context,callback) &&
            xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();xl6ClosureRuntimeV1Get()->Release(callback);
    }
    if(call->fail){if(!call->noDiagnostic)xrtSetErrorKind(XERR_RANGE);return false;}
    *output=!call->reject;return true;
}
static void languageDrop(LanguageCall* call)
{xl6X509CrlEntryDestroyV1(call->savedEntry);xl6X509ExtensionDestroyV1(call->savedExtension);call->savedEntry=NULL;call->savedExtension=NULL;}
static void languageCallbacks(void)
{
    void* crl=xl6X509CrlCreateSpanV1(policy_critical_view);void* issuer=xl6X509CertificateCreateSpanV1(policy_issuer_view);
    void* context=xl6X509CrlCriticalCreateV1();assert(crl && issuer && context);
    FAULTS(xl6X509CrlCriticalCreateV1(),xl6X509CrlCriticalDestroyV1);
    assert(!xl6X509CrlCriticalExtensionV1(context) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    const XL6_ClosureRuntimeV1* closures=xl6ClosureRuntimeV1Get();
    LanguageCall call={.context=context,.crl=crl,.issuer=issuer,.nested=true};
    XL6_Closure* callback=closures->Create(NULL,(void (*)(void))language,NULL,&call,borrowedEnvironmentDrop,NULL,NULL,0,borrowedEnvironmentTrace);assert(callback);
    void* validated=xl6X509CrlValidateCallbackV1(crl,issuer,policy_time,0,31,context,callback);
    assert(validated && call.calls==2 && call.scopeChecked && call.savedEntry && call.savedExtension);
    xl6X509ValidatedCrlDestroyV1(validated);languageDrop(&call);
    call.nested=false;call.reject=true;
    assert(!xl6X509CrlValidateCallbackV1(crl,issuer,policy_time,0,31,context,callback) && xrtGetError());xrtClearError();languageDrop(&call);
    call.reject=false;call.fail=true;xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6X509CrlValidateCallbackV1(crl,issuer,policy_time,0,31,context,callback));
    xl6_error_result failed=xl6ErrorScopeEnd(&scope);assert(!failed.ok && xrtErrorIs(failed.error,XERR_RANGE));xrtErrorFree(failed.error);xrtClearError();languageDrop(&call);
    call.noDiagnostic=true;assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6X509CrlValidateCallbackV1(crl,issuer,policy_time,0,31,context,callback));
    failed=xl6ErrorScopeEnd(&scope);assert(!failed.ok && !failed.error && !xrtGetError());languageDrop(&call);
    call.noDiagnostic=false;
    call.fail=false;
    for(size_t prefix=0;prefix<128;++prefix) {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xl6ErrorScopeBegin(&scope));assert(xrtMemDebugFailAfter(prefix));
        void* result=xl6X509CrlValidateCallbackV1(crl,issuer,policy_time,0,31,context,callback);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;if(hit)++failures;
        if(!result)xl6ErrorScopeCaptureFailure(&scope);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);assert(hit?(!result && !outcome.ok):(result && outcome.ok));
        xrtErrorFree(outcome.error);xrtClearError();languageDrop(&call);xl6X509ValidatedCrlDestroyV1(result);balanced(before);
        if(!hit)break;
        assert(prefix<127);
    }
    validated=xl6X509CrlValidateCallbackV1(crl,issuer,policy_time,0,31,context,callback);assert(validated);
    closures->Release(callback);xl6X509CrlCriticalDestroyV1(context);xl6X509CrlDestroyV1(crl);xl6X509CertificateDestroyV1(issuer);
    assert(call.savedEntry && call.savedExtension && xl6X509CrlEntryVersionV1(call.savedEntry)==2);
    xvalue* encoded=xl6X509ExtensionValueV1(call.savedExtension);assert(encoded);xrtValueRelease(encoded);
    languageDrop(&call);xl6X509ValidatedCrlDestroyV1(validated);
}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(!xrtGetError());
    ordinary();callbacks();languageCallbacks();balanced(before);assert(!xrtGetError());
    printf("X509 CRL policy: 7 ordinary CRLs, %zu allocation budgets, %zu failures, %zu publication budgets; balanced\n",budgets,failures,publications);return 0;
}
