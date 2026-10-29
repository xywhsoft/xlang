#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoP384SharedV1(void* raw,const xvalue* peerValue)
{
    const XL6_CryptoP384KeyV1* self=raw;xbytesview peer;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6CryptoNistMaterial(peerValue,97,&peer)) return NULL;
    void* output=xrtMalloc(48);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,48,xrtP384Shared(self->privateKey,peer.Data,output));
}
