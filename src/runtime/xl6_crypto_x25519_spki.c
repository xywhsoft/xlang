#include "xl6_key_object_internal.h"
#include "xl6_key_abi.h"
#include "xl6_crypto_xdh_internal.h"

bool xl6CryptoX25519WriteSpkiSpanV1(void* raw,bool unused,xbuffer* output)
{
    (void)unused;const XL6_CryptoX25519KeyV1* self=raw;
    if (!self) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_MATERIAL_V1,"missing key receiver");
    return xl6KeyObjectOutcomeV1(xl6KeySpkiWriteV1(
        (xbytesview){self->publicKey,sizeof(self->publicKey)},XL6_KEY_X25519_V1,output));
}
xvalue* xl6CryptoX25519PublicSpkiV1(void* raw)
{
    xbuffer output={0};bool ok=xl6CryptoX25519WriteSpkiSpanV1(raw,false,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
