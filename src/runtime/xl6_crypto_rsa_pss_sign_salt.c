#include "xl6_crypto_rsa_private_internal.h"
#include "xl6_crypto_abi.h"
static xvalue* sign(void* raw,const xvalue* digestValue,const xvalue* saltValue,int64_t algorithm,int64_t maskAlgorithm)
{
    const XL6_CryptoRsaPrivateV1* self=xl6CryptoRsaPrivateChecked(raw);if(!self)return NULL;
    xbytesview digest,salt;xcryptohash hash,mask;size_t ignored;
    if(!xl6CryptoRsaDigest(digestValue,algorithm,&hash,&digest)||
        !xl6CryptoRsaHash(maskAlgorithm,&mask,&ignored)||!xl6CryptoMaterial(saltValue,&salt))return NULL;
    void* output=xrtMalloc(self->key.Public.ModulusSize);if(!output)return NULL;
    return xl6CryptoTakeSecret(output,self->key.Public.ModulusSize,
        xrtRsaPssSignSalt(&self->key,hash,mask,salt.Data,salt.Size,digest.Data,output));
}
xvalue* xl6CryptoRsaSignPssHashSaltV1(void* raw,const xvalue* digestValue,const xvalue* saltValue,int64_t algorithm)
{return sign(raw,digestValue,saltValue,algorithm,algorithm);}
xvalue* xl6CryptoRsaSignPssHashSaltWithMaskV1(void* raw,const xvalue* digestValue,const xvalue* saltValue,int64_t algorithm,int64_t maskAlgorithm)
{return sign(raw,digestValue,saltValue,algorithm,maskAlgorithm);}
