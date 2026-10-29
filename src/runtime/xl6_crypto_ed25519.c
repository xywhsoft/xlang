#include "xl6_crypto_ed25519_internal.h"
#include "xl6_crypto_abi.h"
#include "xl6_crypto_key_spans.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Immutable resident key: cache expansion once, retain the original seed only
 * for explicit export. No language edge or generated address is retained. */
void xl6CryptoEd25519DestroyV1(void* raw)
{
    XL6_CryptoEd25519KeyV1* self=raw;
    if (self) {xrtEd25519KeyClear(&self->expanded);xl6CryptoFreeSecret(self,sizeof(*self));}
}
void* xl6CryptoEd25519CreateSpanV1(xbytesview seed)
{
    if (!seed.Data || seed.Size!=XRT_ED25519_SEED_SIZE) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_CryptoEd25519KeyV1* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    memcpy(self->seed,seed.Data,sizeof(self->seed));
    if (!xrtEd25519KeyInit(&self->expanded,self->seed)) {xl6CryptoEd25519DestroyV1(self);return NULL;}
    return self;
}
void* xl6CryptoEd25519CreateV1(const xvalue* seedValue)
{
    xbytesview seed;
    return xl6CryptoEd25519Material(seedValue,XRT_ED25519_SEED_SIZE,&seed)?xl6CryptoEd25519CreateSpanV1(seed):NULL;
}
xvalue* xl6CryptoEd25519PublicV1(void* raw)
{
    const XL6_CryptoEd25519KeyV1* self=raw;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6CryptoCopySecret(self->expanded.Public,sizeof(self->expanded.Public));
}
xvalue* xl6CryptoEd25519ExportSeedV1(void* raw)
{
    const XL6_CryptoEd25519KeyV1* self=raw;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6CryptoCopySecret(self->seed,sizeof(self->seed));
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoEd25519HandlePolicyV1Get,xl6CryptoEd25519DestroyV1)
