#include "xl6_tls_identity_internal.h"
#include "xl6_key_curve_codecs.h"

void* xl6TlsIdentityRsaV1(XL6_NativeClassArrayContextV1 context,const xvalue* certificates,void* key,
    int64_t maxCertificates,int64_t maxBytes)
{
    if(xrtGetError())return NULL;
    if(!key){xl6TlsFailV1(XERR_ARGUMENT,"missing RSA private key");return NULL;}
    if(!xl6TlsLimitV1(0,maxCertificates,"invalid certificate count budget") ||
        !xl6TlsLimitV1(0,maxBytes,"invalid certificate byte budget"))return NULL;
    xbuffer encoded={0};void* result=NULL;
    if(xl6TlsOutcomeV1(xl6CryptoRsaPrivateWritePkcs1SpanV1(key,false,&encoded)))
        result=xl6TlsIdentityFromMaterialV1(context,certificates,xrtBufferView(&encoded),maxCertificates,maxBytes,xrtTlsIdentityRsa);
    /* The DER bridge is private, short lived and wiped by allocation capacity. */
    xl6KeyBufferFreeV1(&encoded);return result;
}
