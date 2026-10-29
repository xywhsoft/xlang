#include "xl6_x509_path_array_internal.h"
void* xl6X509PathArrayRunV1(XL6_NativeClassArrayContextV1 context,const xvalue* array,void* anchor,void* rawPolicy,
    xx509criticalproc critical,void* data)
{
    if(!anchor || !rawPolicy){xl6X509FailV1(XERR_ARGUMENT,"path check needs anchor and explicit policy");return NULL;}
    const XL6_X509PathPolicyV1* policy=rawPolicy;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    XL6_X509CertificatesV1 input={0};void* result=NULL;
    bool ok=xl6X509CertificatesCaptureV1(&input,context,array,1,policy->maxCertificates);
    if(xrtGetError() || scope.failed)ok=false;
    if(ok){
        xx509pathconfig config=policy->value;config.Critical=critical;config.UserData=data;
        result=xl6X509PathValidateCreateV1(input.raw,input.count,anchor,&config,policy->maxCertificates,policy->maxBytes);ok=result!=NULL;
    }
    if(!ok || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6X509CertificatesReleaseV1(&input,&scope);xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6X509PathOutcomeDestroyV1(result);result=NULL;xl6ErrorScopePropagateFailure(outcome);}
    else xrtErrorFree(outcome.error);
    return result;
}
