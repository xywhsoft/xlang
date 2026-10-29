#include "xl6_crypto_rsa_public_internal.h"
#include "xl6_crypto_abi.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
void xl6CryptoRsaPublicDestroyV1(void* raw)
{
    XL6_CryptoRsaPublicV1* self=raw;if(self)xl6CryptoFreeSecret(self,self->allocation);
}
void* xl6CryptoRsaPublicCopyV1(const xrsapublickey* key)
{
    if (!key || key->ModulusSize<XRT_RSA_MODULUS_MIN_SIZE || key->ModulusSize>XRT_RSA_MAX_MODULUS_SIZE ||
        !key->ExponentSize || key->ExponentSize>key->ModulusSize) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    uint8 input[XRT_RSA_MAX_MODULUS_SIZE]={0},output[XRT_RSA_MAX_MODULUS_SIZE]={0};
    /* Let the public native operation validate its own integer/key contract. */
    bool ok=xrtRsaPublic(key,input,key->ModulusSize,output);
    xrtSecureZero(output,sizeof(output));if (!ok) return NULL;
    size_t size=sizeof(XL6_CryptoRsaPublicV1)+key->ModulusSize+key->ExponentSize;
    XL6_CryptoRsaPublicV1* self=xrtCalloc(1,size);if (!self) return NULL;
    self->allocation=size;self->key=*key;
    memcpy(self->data,key->Modulus,key->ModulusSize);
    memcpy(self->data+key->ModulusSize,key->Exponent,key->ExponentSize);
    self->key.Modulus=self->data;self->key.Exponent=self->data+key->ModulusSize;return self;
}
void* xl6CryptoRsaPublicCreateV1(const xvalue* modulus,const xvalue* exponent)
{
    xbytesview n,e;if(!xl6CryptoRsaRead(modulus,&n)||!xl6CryptoRsaRead(exponent,&e))return NULL;
    xrsapublickey key={n.Data,n.Size,e.Data,e.Size};return xl6CryptoRsaPublicCopyV1(&key);
}
xvalue* xl6CryptoRsaPublicModulusV1(void* raw)
{const XL6_CryptoRsaPublicV1* self=xl6CryptoRsaPublicChecked(raw);return self?xl6CryptoCopySecret(self->key.Modulus,self->key.ModulusSize):NULL;}
xvalue* xl6CryptoRsaPublicExponentV1(void* raw)
{const XL6_CryptoRsaPublicV1* self=xl6CryptoRsaPublicChecked(raw);return self?xl6CryptoCopySecret(self->key.Exponent,self->key.ExponentSize):NULL;}
int64_t xl6CryptoRsaPublicSizeV1(void* raw)
{const XL6_CryptoRsaPublicV1* self=xl6CryptoRsaPublicChecked(raw);return self?(int64_t)self->key.ModulusSize:0;}
int64_t xl6CryptoRsaPublicBitsV1(void* raw)
{const XL6_CryptoRsaPublicV1* self=xl6CryptoRsaPublicChecked(raw);return self?(int64_t)xl6CryptoRsaBits(&self->key):0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoRsaPublicHandlePolicyV1Get,xl6CryptoRsaPublicDestroyV1)
