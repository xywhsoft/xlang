#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoP256SharedV1(void* raw,const xvalue* peerValue)
{
    const XL6_CryptoP256KeyV1* self=raw;xbytesview peer;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6CryptoNistMaterial(peerValue,65,&peer)) return NULL;
    void* output=xrtMalloc(32);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,32,xrtP256Shared(self->privateKey,peer.Data,output));
}
