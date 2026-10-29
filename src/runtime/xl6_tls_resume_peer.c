#include "xl6_tls_internal.h"
#include "xl6_x509_path_array_internal.h"

int64_t xl6TlsResumePeerCertificateCountV1(const void* raw)
{
    if(xrtGetError())return 0;
    size_t count=xrtTlsResumePeerCertificateCountV1(raw);
    if(count>INT64_MAX){xl6TlsFailV1(XERR_RANGE,"TLS resume chain exceeds language integer range");return 0;}
    return (int64_t)count;
}

void* xl6TlsResumePeerCertificateV1(const void* raw,int64_t index)
{
    if(xrtGetError())return NULL;
    if(index<0 || (uint64_t)index>SIZE_MAX){xl6TlsFailV1(XERR_RANGE,"TLS resume certificate index is out of range");return NULL;}
    const xx509cert* certificate=xrtTlsResumePeerCertificateV1(raw,(size_t)index);
    return certificate?xl6X509CertificateCreateSpanV1(certificate->Raw):NULL;
}

void* xl6TlsResumeWithPeerCertificatesV1(XL6_NativeClassArrayContextV1 context,void* raw,
    const xvalue* array,int64_t maxCertificates,int64_t maxBytes)
{
    if(xrtGetError())return NULL;
    if(!raw){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS resume");return NULL;}
    if(!xl6TlsLimitV1(0,maxCertificates,"invalid resume certificate count budget") ||
        !xl6TlsLimitV1(0,maxBytes,"invalid resume certificate byte budget"))return NULL;
    if((uint64_t)maxCertificates>SIZE_MAX || (uint64_t)maxBytes>SIZE_MAX){
        xl6TlsFailV1(XERR_RANGE,"TLS resume certificate budget exceeds native size range");return NULL;
    }
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;
    }
    XL6_X509CertificatesV1 input={0};xbytesview local[8],*views=local;
    xtlsresume* result=NULL;
    bool ok=xl6X509CertificatesCaptureV1(&input,context,array,1,maxCertificates);
    if(ok && input.count>SIZE_MAX/sizeof(*views))ok=xl6TlsFailV1(XERR_RANGE,"TLS resume chain index overflows");
    if(ok && input.count>sizeof(local)/sizeof(*local)){
        views=xrtCalloc(input.count,sizeof(*views));ok=views!=NULL;
    }
    for(size_t i=0;ok && i<input.count;++i)
        views[i]=((const XL6_X509CertificateV1*)input.raw[i])->view.Raw;
    if(ok && !scope.failed && !xrtGetError()){
        xtlsverifylimits limits={0};
        limits.MaxPeerCertificates=(size_t)maxCertificates;limits.MaxPeerBytes=(size_t)maxBytes;
        result=xrtTlsResumeWithPeerCertificatesV1(raw,views,input.count,&limits);
        ok=xl6TlsOutcomeV1(result!=NULL);
    }
    if(!ok || scope.failed || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    if(views!=local){xrtFree(views);if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);}
    xl6X509CertificatesReleaseV1(&input,&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xrtTlsResumeRelease(result);result=NULL;xl6ErrorScopePropagateFailure(outcome);}
    else xrtErrorFree(outcome.error);
    return result;
}
