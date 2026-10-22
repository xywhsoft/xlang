#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
#include "xl6_crypto_key_spans.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Immutable owned native payload: no language edges or generated addresses. */
void xl6CryptoP384DestroyV1(void* raw) {xl6CryptoFreeSecret(raw,sizeof(XL6_CryptoP384KeyV1));}
void* xl6CryptoP384CreateSpanV1(xbytesview key)
{
    if (!key.Data || key.Size!=48) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_CryptoP384KeyV1* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    memcpy(self->privateKey,key.Data,sizeof(self->privateKey));
    if (!xrtP384Public(self->privateKey,self->publicKey)) {xl6CryptoP384DestroyV1(self);return NULL;}
    return self;
}
void* xl6CryptoP384CreateV1(const xvalue* privateValue)
{
    xbytesview key;
    return xl6CryptoNistMaterial(privateValue,48,&key)?xl6CryptoP384CreateSpanV1(key):NULL;
}
xvalue* xl6CryptoP384PublicV1(void* raw)
{
    const XL6_CryptoP384KeyV1* self=raw;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6CryptoCopySecret(self->publicKey,sizeof(self->publicKey));
}
xvalue* xl6CryptoP384ExportPrivateV1(void* raw)
{
    const XL6_CryptoP384KeyV1* self=raw;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6CryptoCopySecret(self->privateKey,sizeof(self->privateKey));
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoP384HandlePolicyV1Get,xl6CryptoP384DestroyV1)
