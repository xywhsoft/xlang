#include "xl6_x509_verify_internal.h"

static int64_t verify(void* raw,void* issuer,bool parsedKey,bool strict)
{
    if(!raw || !issuer){xl6X509FailV1(XERR_ARGUMENT,"signature verification requires a certificate and issuer");return XL6_X509_SIGNATURE_ERROR_V1;}
    const xx509cert* certificate=&((const XL6_X509CertificateV1*)raw)->view;
    xerror* previous=xrtTakeError();
    bool ok=parsedKey?xrtX509CertificateVerifyKey(certificate,&((const XL6_X509PublicKeyInfoV1*)issuer)->value):
        xrtX509CertificateVerify(certificate,&((const XL6_X509CertificateV1*)issuer)->view);
    return xl6X509SignatureFinishV1(ok,previous,strict);
}
int64_t xl6X509CertificateCheckSignatureV1(void* raw,void* issuer){return verify(raw,issuer,false,false);}
int64_t xl6X509CertificateCheckSignatureKeyV1(void* raw,void* key){return verify(raw,key,true,false);}
void xl6X509CertificateVerifySignatureV1(void* raw,void* issuer){(void)verify(raw,issuer,false,true);}
void xl6X509CertificateVerifySignatureKeyV1(void* raw,void* key){(void)verify(raw,key,true,true);}
