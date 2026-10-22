#include "xl6_crypto_xdh_internal.h"
#include "xl6_crypto_abi.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Immutable resident key. It owns no language objects or generated addresses,
 * and may be read concurrently while its language owner remains alive. */
void xl6CryptoX448DestroyV1(void* raw)
{xl6CryptoFreeSecret(raw,sizeof(XL6_CryptoX448KeyV1));}

void* xl6CryptoX448CreateV1(const xvalue* value)
{
    xbytesview key;
    if (!xl6CryptoXdhMaterial(value,56,&key)) return NULL;
    XL6_CryptoX448KeyV1* self=xrtCalloc(1,sizeof(*self));
    if (!self) return NULL;
    memcpy(self->privateKey,key.Data,56);
    if (!xrtX448Public(self->privateKey,self->publicKey)) {
        xl6CryptoX448DestroyV1(self);return NULL;
    }
    return self;
}
xvalue* xl6CryptoX448PublicV1(void* raw)
{
    const XL6_CryptoX448KeyV1* self=raw;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6CryptoCopySecret(self->publicKey,56);
}
xvalue* xl6CryptoX448ExportPrivateV1(void* raw)
{
    const XL6_CryptoX448KeyV1* self=raw;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6CryptoCopySecret(self->privateKey,56);
}
xvalue* xl6CryptoX448SharedV1(void* raw,const xvalue* peerValue)
{
    const XL6_CryptoX448KeyV1* self=raw;xbytesview peer;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6CryptoXdhMaterial(peerValue,56,&peer)) return NULL;
    void* output=xrtMalloc(56);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,56,
        xrtX448Shared(self->privateKey,peer.Data,output));
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoX448HandlePolicyV1Get,xl6CryptoX448DestroyV1)
