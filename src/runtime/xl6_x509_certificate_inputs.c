#include "xl6_x509_path_array_internal.h"
bool xl6X509CertificatesCaptureV1(XL6_X509CertificatesV1* self,XL6_NativeClassArrayContextV1 context,
    const xvalue* array,size_t minimum,int64_t maximum)
{
    XL6_NativeClassArrayPinV1 pin={0};
    bool ok=xl6NativeClassArrayBeginV1(&pin,context,array,xl6X509CertificateHandlePolicyV1Get());
    if(ok){
        self->count=xrtTypedArrayCount(xl6SequenceStorageV1(pin.owner));
        ok=self->count>=minimum || xl6X509FailV1(XERR_ARGUMENT,"too few certificate inputs");
        if(ok)ok=xl6X509LimitV1(self->count,maximum,"certificate input count exceeds budget");
        if(ok && (self->count>INT64_MAX || self->count>SIZE_MAX/(sizeof(void*)+sizeof(xvalue*))))
            ok=xl6X509FailV1(XERR_RANGE,"certificate input index overflow");
    }
    if(ok && self->count){
        self->raw=xrtCalloc(self->count,sizeof(void*)+sizeof(xvalue*));ok=self->raw!=NULL;
        if(ok)self->held=(xvalue**)(self->raw+self->count);
    }
    for(size_t i=0;ok && i<self->count;++i){
        self->held[i]=xl6NativeClassArrayRetainAtV1(&pin,i,&self->raw[i]);
        ok=self->held[i]!=NULL;if(ok)self->captured=i+1;
    }
    /* Every native view now has an independent owning handle. The source
     * collection is never BUSY across a language callback. */
    xl6NativeClassArrayEndV1(&pin);return ok && !xrtGetError();
}
void xl6X509CertificatesReleaseV1(XL6_X509CertificatesV1* self,xl6_error_scope* scope)
{
    for(size_t i=0;i<self->captured;++i){xrtValueRelease(self->held[i]);if(xrtGetError())xl6ErrorScopeCaptureFailure(scope);}
    xrtFree(self->raw);*self=(XL6_X509CertificatesV1){0};
    if(xrtGetError())xl6ErrorScopeCaptureFailure(scope);
}
