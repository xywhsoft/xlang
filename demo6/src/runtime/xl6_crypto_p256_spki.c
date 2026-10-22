#include "xl6_key_object_internal.h"
#include "xl6_key_abi.h"
#include "xl6_crypto_nist_internal.h"

bool xl6CryptoP256WriteSpkiSpanV1(void* raw,bool unused,xbuffer* output)
{
    (void)unused;const XL6_CryptoP256KeyV1* self=raw;
    if (!self) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_MATERIAL_V1,"missing key receiver");
    return xl6KeyObjectOutcomeV1(xl6KeySpkiWriteV1(
        (xbytesview){self->publicKey,sizeof(self->publicKey)},XL6_KEY_P256_V1,output));
}
xvalue* xl6CryptoP256PublicSpkiV1(void* raw)
{
    xbuffer output={0};bool ok=xl6CryptoP256WriteSpkiSpanV1(raw,false,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
