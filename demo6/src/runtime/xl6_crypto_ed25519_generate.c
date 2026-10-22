#include "xl6_crypto_ed25519_internal.h"
#include "xl6_crypto_abi.h"

void* xl6CryptoEd25519GenerateV1(void)
{
    XL6_CryptoEd25519KeyV1* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    /* Native OS randomness followed by one native expansion. Going through
     * seed/public KeyPair and then KeyInit would compute the public key twice. */
    if (!xrtSecureRandom(self->seed,sizeof(self->seed)) ||
        !xrtEd25519KeyInit(&self->expanded,self->seed)) {
        xl6CryptoEd25519DestroyV1(self);return NULL;
    }
    return self;
}
