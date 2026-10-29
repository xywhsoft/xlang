#include "xl6_key_curve_codecs.h"

void* xl6CryptoRsaPublicFromSpkiSpanV1(xbytesview input)
{
    xbytesview publicKey;
    return xl6KeyObjectOutcomeV1(xl6KeySpkiReadV1(input,XL6_KEY_RSA_V1,&publicKey))?
        xl6CryptoRsaPublicFromPkcs1SpanV1(publicKey):NULL;
}
void* xl6CryptoRsaPublicFromSpkiV1(const xvalue* value)
{
    xbytesview input;
    return xl6KeyObjectInputV1(value,&input)?xl6CryptoRsaPublicFromSpkiSpanV1(input):NULL;
}
bool xl6CryptoRsaPublicWriteSpkiSpanV1(void* raw,bool unused,xbuffer* output)
{
    (void)unused;xbuffer publicKey={0};
    bool ok=xl6CryptoRsaPublicWritePkcs1SpanV1(raw,false,&publicKey) &&
        xl6KeySpkiWriteV1(xrtBufferView(&publicKey),XL6_KEY_RSA_V1,output);
    xl6KeyBufferFreeV1(&publicKey);return xl6KeyObjectOutcomeV1(ok);
}
xvalue* xl6CryptoRsaPublicToSpkiV1(void* raw)
{
    xbuffer output={0};bool ok=xl6CryptoRsaPublicWriteSpkiSpanV1(raw,false,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
