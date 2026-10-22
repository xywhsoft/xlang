#include "xl6_crypto_rsa_private_internal.h"
#include "xl6_crypto_abi.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
void xl6CryptoRsaPrivateDestroyV1(void* raw)
{
    XL6_CryptoRsaPrivateV1* self=raw;if(self)xl6CryptoFreeSecret(self,self->allocation);
}
void* xl6CryptoRsaPrivateFromPartsV1(const xbytesview parts[8])
{
    size_t size=sizeof(XL6_CryptoRsaPrivateV1);bool any=false,all=true;
    for(size_t i=0;i<8;++i) {
        if(parts[i].Size>XRT_RSA_MAX_MODULUS_SIZE || (parts[i].Size&&!parts[i].Data)) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
        size+=parts[i].Size;if(i>=3){any|=parts[i].Size!=0;all&=parts[i].Size!=0;}
    }
    if(parts[0].Size<XRT_RSA_MODULUS_MIN_SIZE || !parts[1].Size ||
        parts[1].Size>parts[0].Size || (any&&!all) || (!all&&!parts[2].Size)) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_CryptoRsaPrivateV1* self=xrtCalloc(1,size);if(!self)return NULL;self->allocation=size;
    const void** data[8]={&self->key.Public.Modulus,&self->key.Public.Exponent,&self->key.PrivateExponent,
        &self->key.Prime1,&self->key.Prime2,&self->key.Exponent1,&self->key.Exponent2,&self->key.Coefficient};
    size_t* sizes[8]={&self->key.Public.ModulusSize,&self->key.Public.ExponentSize,&self->key.PrivateExponentSize,
        &self->key.Prime1Size,&self->key.Prime2Size,&self->key.Exponent1Size,&self->key.Exponent2Size,&self->key.CoefficientSize};
    uint8* next=self->data;
    for(size_t i=0;i<8;++i){*sizes[i]=parts[i].Size;*data[i]=parts[i].Size?next:NULL;if(parts[i].Size)memcpy(next,parts[i].Data,parts[i].Size);next+=parts[i].Size;}
    /* Same native operational validation used by XRT TLS identities: private
     * operation plus native public re-verification, with fresh native blinding.
     * This is not a claim of exhaustive mathematical key certification. */
    uint8 input[XRT_RSA_MAX_MODULUS_SIZE]={0},output[XRT_RSA_MAX_MODULUS_SIZE]={0};
    input[parts[0].Size-1]=2;
    bool ok=xrtRsaPrivate(&self->key,input,parts[0].Size,output);
    xrtSecureZero(input,sizeof(input));xrtSecureZero(output,sizeof(output));
    if(!ok){xl6CryptoRsaPrivateDestroyV1(self);return NULL;}return self;
}
void* xl6CryptoRsaPrivateCreateV1(const xvalue* modulus,const xvalue* exponent,const xvalue* privateExponent)
{
    xbytesview parts[8]={0};
    if(!xl6CryptoRsaRead(modulus,&parts[0])||!xl6CryptoRsaRead(exponent,&parts[1])||
        !xl6CryptoRsaRead(privateExponent,&parts[2]))return NULL;
    return xl6CryptoRsaPrivateFromPartsV1(parts);
}
void* xl6CryptoRsaPrivateCreateCrtV1(const xvalue* modulus,const xvalue* exponent,const xvalue* privateExponent,
    const xvalue* prime1,const xvalue* prime2,const xvalue* exponent1,const xvalue* exponent2,const xvalue* coefficient)
{
    const xvalue* values[8]={modulus,exponent,privateExponent,prime1,prime2,exponent1,exponent2,coefficient};
    xbytesview parts[8];for(size_t i=0;i<8;++i)if(!xl6CryptoRsaRead(values[i],&parts[i]))return NULL;
    return xl6CryptoRsaPrivateFromPartsV1(parts);
}
void* xl6CryptoRsaPrivatePublicV1(void* raw)
{
    const XL6_CryptoRsaPrivateV1* self=xl6CryptoRsaPrivateChecked(raw);
    return self?xl6CryptoRsaPublicCopyV1(&self->key.Public):NULL;
}
xvalue* xl6CryptoRsaPrivateExportPartV1(void* raw,int64_t part)
{
    const XL6_CryptoRsaPrivateV1* self=xl6CryptoRsaPrivateChecked(raw);if(!self)return NULL;
    const xbytesview views[6]={{self->key.PrivateExponent,self->key.PrivateExponentSize},
        {self->key.Prime1,self->key.Prime1Size},{self->key.Prime2,self->key.Prime2Size},
        {self->key.Exponent1,self->key.Exponent1Size},{self->key.Exponent2,self->key.Exponent2Size},
        {self->key.Coefficient,self->key.CoefficientSize}};
    if(part<0||part>=6){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6CryptoCopySecret(views[part].Data,views[part].Size);
}
bool xl6CryptoRsaPrivateHasCrtV1(void* raw)
{const XL6_CryptoRsaPrivateV1* self=xl6CryptoRsaPrivateChecked(raw);return self&&self->key.Prime1Size!=0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoRsaPrivateHandlePolicyV1Get,xl6CryptoRsaPrivateDestroyV1)
