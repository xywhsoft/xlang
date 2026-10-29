#include "xl6_crypto_rsa_private_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoRsaSignPkcs1HashV1(void* raw,const xvalue* digestValue,int64_t algorithm)
{
    const XL6_CryptoRsaPrivateV1* self=xl6CryptoRsaPrivateChecked(raw);if(!self)return NULL;
    xbytesview digest;xcryptohash hash;if(!xl6CryptoRsaDigest(digestValue,algorithm,&hash,&digest))return NULL;
    void* output=xrtMalloc(self->key.Public.ModulusSize);if(!output)return NULL;
    return xl6CryptoTakeSecret(output,self->key.Public.ModulusSize,xrtRsaPkcs1Sign(&self->key,hash,digest.Data,output));
}
