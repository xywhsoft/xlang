#include "xl6_x509_path_build_internal.h"
void* xl6X509PathBuildArrayRunV1(XL6_NativeClassArrayContextV1 context,void* target,const xvalue* array,void* trust,void* rawPolicy,
    int64_t maxCandidates,int64_t maxSourceBytes,xx509criticalproc critical,void* data)
{
    if(!target || !trust || !rawPolicy){xl6X509FailV1(XERR_ARGUMENT,"path build needs target, stable trust and explicit policy");return NULL;}
    const XL6_X509PathPolicyV1* policy=rawPolicy;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    XL6_X509CertificatesV1 input={0};void* result=NULL;
    bool ok=xl6X509CertificatesCaptureV1(&input,context,array,0,maxCandidates);
    if(xrtGetError() || scope.failed)ok=false;
    if(ok){
        xx509pathconfig config=policy->value;config.Critical=critical;config.UserData=data;
        result=xl6X509PathBuildCreateV1(target,input.raw,input.count,trust,&config,policy->maxCertificates,policy->maxBytes,maxCandidates,maxSourceBytes);
        ok=result!=NULL;
    }
    if(!ok || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6X509CertificatesReleaseV1(&input,&scope);xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6X509PathOutcomeDestroyV1(result);result=NULL;xl6ErrorScopePropagateFailure(outcome);}
    else xrtErrorFree(outcome.error);
    return result;
}
