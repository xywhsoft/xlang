#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
void* xl6CryptoP256GenerateV1(void)
{
    XL6_CryptoP256KeyV1* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    if (!xrtP256KeyPair(self->privateKey,self->publicKey)) {xl6CryptoP256DestroyV1(self);return NULL;}
    return self;
}
