#include "xl6_tls_identity_internal.h"
#include "xl6_crypto_ed25519_internal.h"
void* xl6TlsIdentityEd25519V1(XL6_NativeClassArrayContextV1 context,const xvalue* certificates,void* raw,
    int64_t maxCertificates,int64_t maxBytes)
{
    if(xrtGetError())return NULL;
    if(!raw){xl6TlsFailV1(XERR_ARGUMENT,"missing Ed25519 key");return NULL;}
    const XL6_CryptoEd25519KeyV1* key=raw;
    return xl6TlsIdentityFromMaterialV1(context,certificates,(xbytesview){key->seed,sizeof(key->seed)},
        maxCertificates,maxBytes,xrtTlsIdentityEd25519);
}
