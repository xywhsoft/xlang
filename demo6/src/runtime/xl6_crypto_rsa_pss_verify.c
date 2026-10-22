#include "xl6_crypto_rsa_verify_internal.h"
#include "xl6_crypto_abi.h"
static bool verify(void* raw,const xvalue* digestValue,const xvalue* signatureValue,
    int64_t algorithm,int64_t maskAlgorithm,int64_t saltLength,bool anySalt)
{
    const XL6_CryptoRsaPublicV1* self=xl6CryptoRsaPublicChecked(raw);if(!self)return false;
    xbytesview digest,signature;xcryptohash hash,mask;size_t ignored;
    if(!xl6CryptoRsaDigest(digestValue,algorithm,&hash,&digest)||
        !xl6CryptoRsaHash(maskAlgorithm,&mask,&ignored)||
        !xl6CryptoRsaSignature(signatureValue,self->key.ModulusSize,&signature))return false;
    size_t salt=0;
    if(anySalt)salt=XRT_RSA_PSS_SALT_ANY;
    else {
        if(saltLength< -1 || (saltLength>0 && (uint64_t)saltLength>SIZE_MAX)){xrtSetErrorKind(XERR_RANGE);return false;}
        salt=saltLength<0?digest.Size:(size_t)saltLength;
        size_t encoded=(xl6CryptoRsaBits(&self->key)-1+7)/8;
        if(encoded<digest.Size+2||salt>encoded-digest.Size-2){xrtSetErrorKind(XERR_RANGE);return false;}
    }
    xerror* previous=xrtTakeError();
    bool ok=xrtRsaPssVerify(&self->key,hash,mask,salt,digest.Data,signature.Data,signature.Size);
    return xl6CryptoRsaVerifyFinish(ok,previous);
}
bool xl6CryptoRsaVerifyPssHashV1(void* raw,const xvalue* digest,const xvalue* signature,int64_t algorithm,int64_t saltLength)
{return verify(raw,digest,signature,algorithm,algorithm,saltLength,false);}
bool xl6CryptoRsaVerifyPssHashWithMaskV1(void* raw,const xvalue* digest,const xvalue* signature,int64_t algorithm,int64_t maskAlgorithm,int64_t saltLength)
{return verify(raw,digest,signature,algorithm,maskAlgorithm,saltLength,false);}
bool xl6CryptoRsaVerifyPssHashAnySaltV1(void* raw,const xvalue* digest,const xvalue* signature,int64_t algorithm)
{return verify(raw,digest,signature,algorithm,algorithm,0,true);}
bool xl6CryptoRsaVerifyPssHashAnySaltWithMaskV1(void* raw,const xvalue* digest,const xvalue* signature,int64_t algorithm,int64_t maskAlgorithm)
{return verify(raw,digest,signature,algorithm,maskAlgorithm,0,true);}
