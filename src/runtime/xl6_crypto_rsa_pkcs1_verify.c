#include "xl6_crypto_rsa_verify_internal.h"
#include "xl6_crypto_abi.h"
bool xl6CryptoRsaVerifyPkcs1HashV1(void* raw,const xvalue* digestValue,const xvalue* signatureValue,int64_t algorithm)
{
    const XL6_CryptoRsaPublicV1* self=xl6CryptoRsaPublicChecked(raw);if(!self)return false;
    xbytesview digest,signature;xcryptohash hash;
    if(!xl6CryptoRsaDigest(digestValue,algorithm,&hash,&digest)||
        !xl6CryptoRsaSignature(signatureValue,self->key.ModulusSize,&signature))return false;
    xerror* previous=xrtTakeError();
    bool ok=xrtRsaPkcs1Verify(&self->key,hash,digest.Data,signature.Data,signature.Size);
    return xl6CryptoRsaVerifyFinish(ok,previous);
}
