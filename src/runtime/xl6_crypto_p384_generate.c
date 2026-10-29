#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
void* xl6CryptoP384GenerateV1(void)
{
    XL6_CryptoP384KeyV1* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    if (!xrtP384KeyPair(self->privateKey,self->publicKey)) {xl6CryptoP384DestroyV1(self);return NULL;}
    return self;
}
