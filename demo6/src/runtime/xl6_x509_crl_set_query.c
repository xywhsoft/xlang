#include "xl6_x509_crl_policy_internal.h"
void* xl6X509CrlSetQueryV1(void* raw,void* certificate)
{
    if(!raw || !certificate){xl6X509FailV1(XERR_ARGUMENT,"CRL set query needs set and certificate");return NULL;}
    const XL6_X509CrlSetV1* self=raw;xx509revocation value={0};
    xx509result result=xrtX509CrlSetCheck(&self->value,&((XL6_X509CertificateV1*)certificate)->view,&value);
    if(!xl6X509OutcomeV1(result!=X509_ERROR))return NULL;
    /* A delta necessarily uses v2. A base entry keeps its base CRL version. */
    xx509crlversion version=self->base->crl.Version;
    if(result==X509_VALUE && value.Entry.Raw.Data) {
        uintptr_t start=(uintptr_t)self->delta->crl.Raw.Data,p=(uintptr_t)value.Entry.Raw.Data;
        if(p>=start && p-start<self->delta->crl.Raw.Size)version=self->delta->crl.Version;
    }
    return xl6X509RevocationCopyV1(&value,version,result==X509_VALUE);
}
