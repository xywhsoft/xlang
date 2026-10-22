#include "xl6_key_curve_codecs.h"
#include "xl6_crypto_rsa_public_internal.h"

void* xl6CryptoRsaPublicFromPkcs1SpanV1(xbytesview input)
{
    xbytesview parts[2];
    if (!xl6KeyObjectOutcomeV1(xl6KeyRsaPublicReadV1(input,parts))) return NULL;
    xrsapublickey key={parts[0].Data,parts[0].Size,parts[1].Data,parts[1].Size};
    void* object=xl6CryptoRsaPublicCopyV1(&key);
    if (!xl6KeyObjectOutcomeV1(object!=NULL)) {xl6CryptoRsaPublicDestroyV1(object);return NULL;}
    return object;
}
void* xl6CryptoRsaPublicFromPkcs1V1(const xvalue* value)
{
    xbytesview input;
    return xl6KeyObjectInputV1(value,&input)?xl6CryptoRsaPublicFromPkcs1SpanV1(input):NULL;
}
bool xl6CryptoRsaPublicWritePkcs1SpanV1(void* raw,bool unused,xbuffer* output)
{
    (void)unused;const XL6_CryptoRsaPublicV1* self=raw;
    if (!self) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_MATERIAL_V1,"missing RSA public key receiver");
    xbytesview parts[2]={{self->key.Modulus,self->key.ModulusSize},{self->key.Exponent,self->key.ExponentSize}};
    return xl6KeyObjectOutcomeV1(xl6KeyRsaPublicWriteV1(parts,output));
}
xvalue* xl6CryptoRsaPublicToPkcs1V1(void* raw)
{
    xbuffer output={0};bool ok=xl6CryptoRsaPublicWritePkcs1SpanV1(raw,false,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
