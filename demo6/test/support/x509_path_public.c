#define main orderedFoundationMain
#include "x509_path_owned.c"
#undef main
#include "src/runtime/xl6_x509_path_critical_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
static xvalue* input(xbytesview target,xbytesview issuer)
{
    uint64_t tag=901;XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,0,&tag,1};
    xvalue* array=xl6SequenceNominalCreateV1(1,701,801,&schema,false,801);assert(array);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(array,1,701,801,3,sizeof(xvalue*),xrtTypeValue());assert(pin && xl6SequenceBindProjectionV1(pin,NULL));
    xbytesview items[2];items[0]=target;items[1]=issuer;
    for(unsigned i=0;i<2;++i) {
        void* raw=xl6X509CertificateCreateSpanV1(items[i]);assert(raw);
        xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6X509CertificateHandlePolicyV1Get());assert(handle && !raw);
        xvalue* object=xrtValueObject();assert(object);
        assert(xrtValueObjectSetNewPostCommit(object,(xstrview){"handle",6},handle));
        assert(xrtValueObjectSetNewPostCommit(object,(xstrview){"$xlir.class.runtime-type",24},xrtValueInt(901)));
        assert(xl6SequenceAdmitSlotV1(pin,&object) && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&object));xrtValueRelease(object);
    }
    xl6SequenceEndV1(pin);return array;
}
static XL6_NativeClassArrayContextV1 arrayContext(void)
{return (XL6_NativeClassArrayContextV1){{701,801,3,sizeof(xvalue*),xrtTypeValue()},"handle",6,xl6X509CertificateHandlePolicyV1Get()};}
typedef struct Call {void* context;void* anchor;void* policy;xvalue* array;void* savedCertificate;void* savedExtension;bool fail,noDiagnostic,nested,clearInput;unsigned calls;} Call;
static void environmentDrop(void* raw){(void)raw;}
static bool environmentTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* data)
{(void)raw;(void)visit;(void)data;return true;}
static bool invoke(void* raw,bool* result)
{
    Call* call=raw;++call->calls;
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(call->array,1,701,801,3,sizeof(xvalue*),xrtTypeValue());
    assert(pin && xrtTypedArrayCount(xl6SequenceStorageV1(pin))==2);
    if(call->clearInput)xrtTypedArrayClear(xl6SequenceStorageV1(pin));
    xl6SequenceEndV1(pin);assert(!xrtGetError());
    void* certificate=xl6X509PathCriticalCertificateV1(call->context);
    if(!certificate)return false;
    void* extension=xl6X509PathCriticalExtensionV1(call->context);
    if(!extension){xl6X509CertificateDestroyV1(certificate);return false;}
    assert(xl6X509PathCriticalDepthV1(call->context)==0);
    xl6X509CertificateDestroyV1(call->savedCertificate);xl6X509ExtensionDestroyV1(call->savedExtension);
    call->savedCertificate=certificate;call->savedExtension=extension;
    if(call->nested) {
        const XL6_ClosureRuntimeV1* closures=xl6ClosureRuntimeV1Get();
        XL6_Closure* callback=closures->Create(NULL,(void (*)(void))invoke,NULL,call,environmentDrop,NULL,NULL,0,environmentTrace);assert(callback);
        assert(!xl6X509PathCheckCallbackV1(arrayContext(),call->array,call->anchor,call->policy,call->context,callback));
        assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();closures->Release(callback);
    }
    if(call->fail){if(!call->noDiagnostic)xrtSetErrorKind(XERR_RANGE);return false;}
    *result=true;return true;
}
static void clear(Call* call)
{
    xl6X509CertificateDestroyV1(call->savedCertificate);xl6X509ExtensionDestroyV1(call->savedExtension);
    call->savedCertificate=NULL;call->savedExtension=NULL;
}
static void ownedCatalog(void* raw,const XL6_NativeHandlePolicyV1* policy,const XL6_OwnershipDomainV1* domain)
{
    assert(raw);xvalue* value=xl6NativeHandleTakeV1(&raw,policy);assert(value && !raw);
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(&domain,1);assert(catalog);
    xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope frozen={0};size_t count=0;
    assert(xrtOwnershipFreezeTryBegin(&frozen) && ref.Ops->Count(ref.Data,&count) && count==1);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&prepare,&finish) && adapter && !prepare && !finish);
    int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&frozen));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);xrtValueRelease(value);
}
#define CATALOG(Call,Part) ownedCatalog(Call,xl6X509##Part##HandlePolicyV1Get(),xl6OwnershipDomainX509##Part##V1Get())
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(!orderedFoundationMain());
    const PathVector* vector=&pathVectors[3];
    void* root=xl6X509CertificateCreateSpanV1(vector->root);assert(root);void* anchor=xl6X509AnchorCreateV1(root);xl6X509CertificateDestroyV1(root);assert(anchor);
    FAULTS(xl6X509PathPolicyCreateV1(pathTime,3,1,(xstrview){"1.3.6.1.5.5.7.3.1",17},64,16777216),xl6X509PathPolicyDestroyV1);
    void* policy=xl6X509PathPolicyCreateV1(pathTime,0,0,(xstrview){"",0},64,16777216);assert(policy);
    xvalue* ordinary=input(vector->leaf,vector->mid);xvalue* custom=input(vector->custom,vector->mid);
    FAULTS(xl6X509PathCheckV1(arrayContext(),ordinary,anchor,policy),xl6X509PathOutcomeDestroyV1);
    void* context=xl6X509PathCriticalCreateV1();assert(context);
    assert(!xl6X509PathCriticalCertificateV1(context) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    Call call={.context=context,.anchor=anchor,.policy=policy,.array=custom,.nested=true};
    const XL6_ClosureRuntimeV1* closures=xl6ClosureRuntimeV1Get();
    XL6_Closure* callback=closures->Create(NULL,(void (*)(void))invoke,NULL,&call,environmentDrop,NULL,NULL,0,environmentTrace);assert(callback);
    void* result=xl6X509PathCheckCallbackV1(arrayContext(),custom,anchor,policy,context,callback);
    assert(result && call.calls==1);xl6X509PathOutcomeDestroyV1(result);clear(&call);call.nested=false;
    for(unsigned mode=0;mode<2;++mode) {
        call.fail=true;call.noDiagnostic=mode!=0;xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
        assert(!xl6X509PathCheckCallbackV1(arrayContext(),custom,anchor,policy,context,callback));
        xl6_error_result failure=xl6ErrorScopeEnd(&scope);assert(!failure.ok && (mode?!failure.error:xrtErrorKind(failure.error)==XERR_RANGE));
        xrtErrorFree(failure.error);clear(&call);
    }
    call.fail=false;
    for(size_t prefix=0;prefix<128;++prefix) {
        xmemdebugsnapshot start;xrtMemDebugSnapshot(&start);xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(xrtMemDebugFailAfter(prefix));
        result=xl6X509PathCheckCallbackV1(arrayContext(),custom,anchor,policy,context,callback);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;if(hit)++failures;
        if(!result)xl6ErrorScopeCaptureFailure(&scope);
        xl6_error_result failure=xl6ErrorScopeEnd(&scope);
        assert(hit?(!result && !failure.ok):(result && failure.ok));xrtErrorFree(failure.error);clear(&call);xl6X509PathOutcomeDestroyV1(result);balanced(start);
        if(!hit)break;
        assert(prefix<127);
    }
    PUBLISH(xl6X509PathPolicyCreateV1(pathTime,0,0,(xstrview){"",0},0,0),xl6X509PathPolicyHandlePolicyV1Get());
    PUBLISH(xl6X509PathCriticalCreateV1(),xl6X509PathCriticalHandlePolicyV1Get());
    CATALOG(xl6X509AnchorCreateV1(&((XL6_X509CertificateV1){.view=((XL6_X509AnchorV1*)anchor)->certificate})),Anchor);
    CATALOG(xl6X509PathPolicyCreateV1(pathTime,0,0,(xstrview){"",0},0,0),PathPolicy);
    result=xl6X509PathCheckV1(arrayContext(),ordinary,anchor,policy);assert(result);
    void* path=xl6X509PathOutcomePathV1(result);assert(path);xl6X509PathOutcomeDestroyV1(result);CATALOG(path,Path);
    CATALOG(xl6X509PathCheckV1(arrayContext(),ordinary,anchor,policy),PathOutcome);
    CATALOG(xl6X509PathCriticalCreateV1(),PathCritical);
    call.clearInput=true;
    result=xl6X509PathCheckCallbackV1(arrayContext(),custom,anchor,policy,context,callback);assert(result);
    closures->Release(callback);xl6X509PathCriticalDestroyV1(context);xl6X509PathPolicyDestroyV1(policy);xl6X509AnchorDestroyV1(anchor);
    xrtValueRelease(ordinary);xrtValueRelease(custom);assert(call.savedCertificate && call.savedExtension);
    xl6X509PathOutcomeDestroyV1(result);clear(&call);balanced(before);
    printf("X509 public path: %zu allocation budgets, %zu failures, %zu publication budgets; 5 catalogs; balanced\n",budgets,failures,publications);return 0;
}
