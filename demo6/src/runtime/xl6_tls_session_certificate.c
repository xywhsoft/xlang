#include "xl6_tls_session_internal.h"
#include "xl6_tls_session_abi.h"
#include "xl6_x509_internal.h"

void* xl6TlsSessionPeerCertificateAtV1(void* raw,int64_t index)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};void* result=NULL;
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return NULL;
    if(!xl6TlsSessionPeerReadyInternalV1(self)){
        if(!xrtGetError())xl6TlsFailV1(XERR_STATE,"TLS peer is not authenticated yet");
    }else{
        size_t count=self->role->certificateCount?self->role->certificateCount(self->native):0;
        if(!xrtGetError()){
            if(index<0||(uint64_t)index>=count)xl6TlsFailV1(XERR_RANGE,"TLS peer certificate index is outside the authenticated chain");
            else if(!self->role->certificate)xl6TlsFailV1(XERR_INTERNAL,"TLS role has no peer certificate reader");
            else {
                const xx509cert* certificate=self->role->certificate(self->native,(size_t)index);
                if(certificate)result=xl6X509CertificateCreateSpanV1(certificate->Raw);
                else if(!xrtGetError())xl6TlsFailV1(XERR_INTERNAL,"TLS peer certificate reader returned no certificate");
            }
        }
    }
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
