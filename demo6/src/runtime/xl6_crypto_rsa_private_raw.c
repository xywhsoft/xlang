#include "xl6_crypto_rsa_private_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoRsaPrivateRawV1(void* raw,const xvalue* value)
{
    const XL6_CryptoRsaPrivateV1* self=xl6CryptoRsaPrivateChecked(raw);if(!self)return NULL;xbytesview input;
    size_t size=self->key.Public.ModulusSize;
    if(!xl6CryptoRsaSignature(value,size,&input))return NULL;
    void* output=xrtMalloc(size);if(!output)return NULL;
    return xl6CryptoTakeSecret(output,size,xrtRsaPrivate(&self->key,input.Data,input.Size,output));
}
