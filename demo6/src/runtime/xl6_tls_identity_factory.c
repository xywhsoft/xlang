#include "xl6_tls_identity_internal.h"
#include "xl6_x509_path_array_internal.h"

void* xl6TlsIdentityFromMaterialV1(XL6_NativeClassArrayContextV1 context,const xvalue* array,
    xbytesview key,int64_t maxCertificates,int64_t maxBytes,XL6_TlsIdentityFactoryV1 factory)
{
    if(xrtGetError())return NULL;
    if(!key.Data || !key.Size || !factory){xl6TlsFailV1(XERR_ARGUMENT,"TLS identity needs a complete private key");return NULL;}
    if(!xl6TlsLimitV1(0,maxCertificates,"invalid certificate count budget") ||
        !xl6TlsLimitV1(0,maxBytes,"invalid certificate byte budget"))return NULL;
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;
    }
    XL6_X509CertificatesV1 input={0};xbytesview local[8],*views=local;
    xtlsidentity* result=NULL;
    bool ok=xl6X509CertificatesCaptureV1(&input,context,array,1,maxCertificates);
    if(ok && (input.count>SIZE_MAX/sizeof(*views)))ok=xl6TlsFailV1(XERR_RANGE,"TLS chain index overflow");
    if(ok && input.count>sizeof(local)/sizeof(*local)){
        views=xrtCalloc(input.count,sizeof(*views));ok=views!=NULL;
    }
    size_t bytes=0;
    for(size_t i=0;ok && i<input.count;++i){
        const xx509cert* certificate=&((const XL6_X509CertificateV1*)input.raw[i])->view;
        views[i]=certificate->Raw;
        if(views[i].Size>SIZE_MAX-bytes)ok=xl6TlsFailV1(XERR_RANGE,"TLS chain byte count overflow");
        else {bytes+=views[i].Size;ok=xl6TlsLimitV1(bytes,maxBytes,"TLS chain exceeds certificate byte budget");}
    }
    if(ok && !scope.failed && !xrtGetError()){
        result=factory(views,input.count,key);ok=xl6TlsOutcomeV1(result!=NULL);
    }
    if(!ok || scope.failed || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    if(views!=local){xrtFree(views);if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);}
    xl6X509CertificatesReleaseV1(&input,&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6TlsIdentityDestroyV1(result);result=NULL;xl6ErrorScopePropagateFailure(outcome);}
    else xrtErrorFree(outcome.error);
    return result;
}
