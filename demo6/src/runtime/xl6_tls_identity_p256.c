#include "xl6_tls_identity_internal.h"
#include "xl6_crypto_nist_internal.h"
void* xl6TlsIdentityP256V1(XL6_NativeClassArrayContextV1 context,const xvalue* certificates,void* raw,
    int64_t maxCertificates,int64_t maxBytes)
{
    if(xrtGetError())return NULL;
    if(!raw){xl6TlsFailV1(XERR_ARGUMENT,"missing P256 private key");return NULL;}
    const XL6_CryptoP256KeyV1* key=raw;
    return xl6TlsIdentityFromMaterialV1(context,certificates,(xbytesview){key->privateKey,sizeof(key->privateKey)},
        maxCertificates,maxBytes,xrtTlsIdentityP256);
}
