#include "xl6_tls_verifier_internal.h"
#include "xl6_x509_path_array_internal.h"
#include "xl6_text_internal.h"
static void verify(XL6_NativeClassArrayContextV1 context,void* raw,const xvalue* certificates,xtlsrole role,xstrview name)
{
    if(xrtGetError())return;
    const XL6_TlsVerifierV1* self=raw;
    if(!self || !self->value || !self->policy){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS verifier");return;}
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return;}
    XL6_X509CertificatesV1 input={0};xx509cert local[4],*views=local;
    bool ok=xl6X509CertificatesCaptureV1(&input,context,certificates,1,(int64_t)self->policy->limits.MaxPeerCertificates);
    size_t bytes=0;
    for(size_t i=0;ok && i<input.count;++i){
        size_t next=((const XL6_X509CertificateV1*)input.raw[i])->view.Raw.Size;
        if(next>SIZE_MAX-bytes)ok=xl6TlsFailV1(XERR_RANGE,"TLS peer certificate bytes overflow");
        else {bytes+=next;ok=xl6TlsLimitV1(bytes,(int64_t)self->policy->limits.MaxPeerBytes,"TLS peer certificate bytes exceed their budget");}
    }
    if(ok && input.count>SIZE_MAX/sizeof(*views))ok=xl6TlsFailV1(XERR_RANGE,"TLS peer view array overflows");
    if(ok && input.count>sizeof(local)/sizeof(*local)){views=xrtCalloc(input.count,sizeof(*views));ok=views!=NULL;}
    for(size_t i=0;ok && i<input.count;++i)views[i]=((const XL6_X509CertificateV1*)input.raw[i])->view;
    if(ok && !scope.failed && !xrtGetError())ok=xl6TlsOutcomeV1(xrtTlsVerifierVerify(self->value,role,name,views,input.count));
    if(!ok || scope.failed || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    if(views!=local){xrtFree(views);if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);}
    xl6X509CertificatesReleaseV1(&input,&scope);xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
}
void xl6TlsVerifierServerV1(XL6_NativeClassArrayContextV1 context,void* raw,const xvalue* certificates,xstrview name,int64_t size)
{
    if(xrtGetError() || !xl6TextSelect(&name,size))return;
    if(!name.Size || name.Size>UINT16_MAX-5u || memchr(name.Data,0,name.Size)){
        xl6TlsFailV1(XERR_VALUE,"TLS peer name must be a nonempty exact DNS or IP view without NUL");return;
    }
    verify(context,raw,certificates,XTLS_SERVER,name);
}
void xl6TlsVerifierClientV1(XL6_NativeClassArrayContextV1 context,void* raw,const xvalue* certificates)
{verify(context,raw,certificates,XTLS_CLIENT,(xstrview){0});}
