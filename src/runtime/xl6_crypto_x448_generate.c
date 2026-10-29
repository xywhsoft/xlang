#include "xl6_crypto_xdh_internal.h"
#include "xl6_crypto_abi.h"

/* Keep the operating-system random source out of import-only programs. */
void* xl6CryptoX448GenerateV1(void)
{
    XL6_CryptoX448KeyV1* self=xrtCalloc(1,sizeof(*self));
    if (!self) return NULL;
    if (!xrtX448KeyPair(self->privateKey,self->publicKey)) {
        xl6CryptoX448DestroyV1(self);return NULL;
    }
    return self;
}
