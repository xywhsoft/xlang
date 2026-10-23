#include "xl6_x509_path_array_internal.h"
void* xl6X509PathArrayRunV1(XL6_NativeClassArrayContextV1 context,const xvalue* array,void* anchor,void* rawPolicy,
    xx509criticalproc critical,void* data)
{
    if(!anchor || !rawPolicy){xl6X509FailV1(XERR_ARGUMENT,"path check needs anchor and explicit policy");return NULL;}
    const XL6_X509PathPolicyV1* policy=rawPolicy;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    XL6_NativeClassArrayPinV1 pin={0};void** raw=NULL;xvalue** held=NULL;size_t count=0,captured=0;void* result=NULL;
    bool ok=xl6NativeClassArrayBeginV1(&pin,context,array,xl6X509CertificateHandlePolicyV1Get());
    if(ok) {
        count=xrtTypedArrayCount(xl6SequenceStorageV1(pin.owner));
        ok=count!=0 || xl6X509FailV1(XERR_ARGUMENT,"empty ordered certification path");
        if(ok)ok=xl6X509LimitV1(count,policy->maxCertificates,"path exceeds certificate budget");
        if(ok && (count>INT64_MAX || count>SIZE_MAX/(sizeof(void*)+sizeof(xvalue*))))
            ok=xl6X509FailV1(XERR_RANGE,"path input index overflow");
    }
    if(ok){raw=xrtCalloc(count,sizeof(void*)+sizeof(xvalue*));ok=raw!=NULL;if(ok)held=(xvalue**)(raw+count);}
    for(size_t i=0;ok && i<count;++i) {
        held[i]=xl6NativeClassArrayRetainAtV1(&pin,i,&raw[i]);ok=held[i]!=NULL;if(ok)captured=i+1;
    }
    /* The certificate handles now own every native input independently.
     * Do not keep the source collection BUSY while invoking user code. */
    xl6NativeClassArrayEndV1(&pin);
    if(xrtGetError() || scope.failed)ok=false;
    if(ok) {
        xx509pathconfig config=policy->value;config.Critical=critical;config.UserData=data;
        result=xl6X509PathValidateCreateV1(raw,count,anchor,&config,policy->maxCertificates,policy->maxBytes);
        ok=result!=NULL;
    }
    if(!ok || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    for(size_t i=0;i<captured;++i){xrtValueRelease(held[i]);if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);}
    xrtFree(raw);xl6NativeClassArrayEndV1(&pin);
    if(xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6X509PathOutcomeDestroyV1(result);result=NULL;xl6ErrorScopePropagateFailure(outcome);}
    else xrtErrorFree(outcome.error);
    return result;
}
