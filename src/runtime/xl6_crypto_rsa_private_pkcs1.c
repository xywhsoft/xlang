#include "xl6_key_curve_codecs.h"
#include "xl6_crypto_rsa_private_internal.h"

void* xl6CryptoRsaPrivateFromPkcs1SpanV1(xbytesview input)
{
    xbytesview parts[8];
    if (!xl6KeyObjectOutcomeV1(xl6KeyRsaPrivateReadV1(input,parts))) return NULL;
    void* object=xl6CryptoRsaPrivateFromPartsV1(parts);
    if (!xl6KeyObjectOutcomeV1(object!=NULL)) {xl6CryptoRsaPrivateDestroyV1(object);return NULL;}
    return object;
}
void* xl6CryptoRsaPrivateFromPkcs1V1(const xvalue* value)
{
    xbytesview input;
    return xl6KeyObjectInputV1(value,&input)?xl6CryptoRsaPrivateFromPkcs1SpanV1(input):NULL;
}
bool xl6CryptoRsaPrivateWritePkcs1SpanV1(void* raw,bool unused,xbuffer* output)
{
    (void)unused;const XL6_CryptoRsaPrivateV1* self=raw;
    if (!self) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_MATERIAL_V1,"missing RSA private key receiver");
    const xrsaprivatekey* key=&self->key;
    if (!key->Prime1Size)
        return xl6KeyFormatFailV1(XERR_UNSUPPORTED,XL6_KEY_FORMAT_MATERIAL_V1,"RSA key format export requires complete CRT components");
    xbytesview parts[8]={{key->Public.Modulus,key->Public.ModulusSize},{key->Public.Exponent,key->Public.ExponentSize},
        {key->PrivateExponent,key->PrivateExponentSize},{key->Prime1,key->Prime1Size},{key->Prime2,key->Prime2Size},
        {key->Exponent1,key->Exponent1Size},{key->Exponent2,key->Exponent2Size},{key->Coefficient,key->CoefficientSize}};
    return xl6KeyObjectOutcomeV1(xl6KeyRsaPrivateWriteV1(parts,output));
}
xvalue* xl6CryptoRsaPrivateToPkcs1V1(void* raw)
{
    xbuffer output={0};bool ok=xl6CryptoRsaPrivateWritePkcs1SpanV1(raw,false,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
