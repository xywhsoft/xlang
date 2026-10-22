#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoP384SignHashV1(void* raw,const xvalue* digestValue,int64_t algorithm)
{
    const XL6_CryptoP384KeyV1* self=raw;xbytesview digest;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6CryptoNistSignDigest(digestValue,algorithm,&digest)) return NULL;
    void* output=xrtMalloc(96);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,96,
        xrtEcdsaP384Sign((xcryptohash)algorithm,digest.Data,self->privateKey,output));
}
