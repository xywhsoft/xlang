#include "xl6_crypto_rsa_private_internal.h"
#include "xl6_crypto_abi.h"
static xvalue* sign(void* raw,const xvalue* digestValue,int64_t algorithm,int64_t maskAlgorithm)
{
    const XL6_CryptoRsaPrivateV1* self=xl6CryptoRsaPrivateChecked(raw);if(!self)return NULL;
    xbytesview digest;xcryptohash hash,mask;size_t ignored;
    if(!xl6CryptoRsaDigest(digestValue,algorithm,&hash,&digest)||
        !xl6CryptoRsaHash(maskAlgorithm,&mask,&ignored))return NULL;
    void* output=xrtMalloc(self->key.Public.ModulusSize);if(!output)return NULL;
    return xl6CryptoTakeSecret(output,self->key.Public.ModulusSize,
        xrtRsaPssSign(&self->key,hash,mask,digest.Data,output));
}
xvalue* xl6CryptoRsaSignPssHashV1(void* raw,const xvalue* digestValue,int64_t algorithm)
{return sign(raw,digestValue,algorithm,algorithm);}
xvalue* xl6CryptoRsaSignPssHashWithMaskV1(void* raw,const xvalue* digestValue,int64_t algorithm,int64_t maskAlgorithm)
{return sign(raw,digestValue,algorithm,maskAlgorithm);}
