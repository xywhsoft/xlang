#include "xl6_crypto_rsa_public_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoRsaPublicRawV1(void* raw,const xvalue* value)
{
    const XL6_CryptoRsaPublicV1* self=xl6CryptoRsaPublicChecked(raw);if(!self)return NULL;xbytesview input;
    size_t size=self->key.ModulusSize;
    if(!xl6CryptoRsaSignature(value,size,&input))return NULL;
    void* output=xrtMalloc(size);if(!output)return NULL;
    return xl6CryptoTakeSecret(output,size,xrtRsaPublic(&self->key,input.Data,input.Size,output));
}
