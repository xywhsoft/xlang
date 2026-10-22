#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoP256SignHashDerV1(void* raw,const xvalue* digestValue,int64_t algorithm)
{
    const XL6_CryptoP256KeyV1* self=raw;xbytesview digest;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6CryptoNistSignDigest(digestValue,algorithm,&digest)) return NULL;
    uint8 output[72]={0};size_t size=0;
    bool ok=xrtEcdsaP256SignDer((xcryptohash)algorithm,digest.Data,self->privateKey,output,sizeof(output),&size);
    xvalue* result=ok?xl6CryptoCopySecret(output,size):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
