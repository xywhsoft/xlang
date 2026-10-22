#include "xl6_key_curve_codecs.h"
#include "xl6_crypto_rsa_private_internal.h"

void* xl6CryptoRsaPrivateFromPkcs8SpanV1(xbytesview input)
{
    xbytesview privateKey,publicKey;
    if (!xl6KeyObjectOutcomeV1(xl6KeyPkcs8ReadV1(input,XL6_KEY_RSA_V1,&privateKey,&publicKey))) return NULL;
    void* object=xl6CryptoRsaPrivateFromPkcs1SpanV1(privateKey);
    if (!object) return NULL;
    if (publicKey.Data) {
        xbytesview encoded[2];const XL6_CryptoRsaPrivateV1* self=object;
        bool ok=xl6KeyRsaPublicReadV1(publicKey,encoded) &&
            xl6KeyObjectPublicMatchesV1(encoded[0],(xbytesview){self->key.Public.Modulus,self->key.Public.ModulusSize}) &&
            xl6KeyObjectPublicMatchesV1(encoded[1],(xbytesview){self->key.Public.Exponent,self->key.Public.ExponentSize});
        if (!xl6KeyObjectOutcomeV1(ok)) {xl6CryptoRsaPrivateDestroyV1(object);return NULL;}
    }
    return object;
}
void* xl6CryptoRsaPrivateFromPkcs8V1(const xvalue* value)
{
    xbytesview input;
    return xl6KeyObjectInputV1(value,&input)?xl6CryptoRsaPrivateFromPkcs8SpanV1(input):NULL;
}
bool xl6CryptoRsaPrivateWritePkcs8SpanV1(void* raw,bool includePublic,xbuffer* output)
{
    xbuffer privateKey={0},publicKey={0};
    bool ok=xl6CryptoRsaPrivateWritePkcs1SpanV1(raw,false,&privateKey);
    if (ok && includePublic) {
        const XL6_CryptoRsaPrivateV1* self=raw;
        xbytesview parts[2]={{self->key.Public.Modulus,self->key.Public.ModulusSize},
                           {self->key.Public.Exponent,self->key.Public.ExponentSize}};
        ok=xl6KeyRsaPublicWriteV1(parts,&publicKey);
    }
    if (ok) ok=xl6KeyPkcs8WriteV1(xrtBufferView(&privateKey),xrtBufferView(&publicKey),XL6_KEY_RSA_V1,output);
    xl6KeyBufferFreeV1(&publicKey);xl6KeyBufferFreeV1(&privateKey);return xl6KeyObjectOutcomeV1(ok);
}
xvalue* xl6CryptoRsaPrivateToPkcs8V1(void* raw,bool includePublic)
{
    xbuffer output={0};bool ok=xl6CryptoRsaPrivateWritePkcs8SpanV1(raw,includePublic,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
