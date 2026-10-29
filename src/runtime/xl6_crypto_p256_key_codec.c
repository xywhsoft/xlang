#include "xl6_key_curve_codecs.h"
#include "xl6_crypto_nist_internal.h"

static xbytesview privateView(const void* raw)
{
    const XL6_CryptoP256KeyV1* self=raw;
    return (xbytesview){self->privateKey,sizeof(self->privateKey)};
}
static xbytesview publicView(const void* raw)
{
    const XL6_CryptoP256KeyV1* self=raw;
    return (xbytesview){self->publicKey,sizeof(self->publicKey)};
}
const XL6_KeyImportV1 xl6CryptoP256ImportSpecV1={
    XL6_KEY_P256_V1,xl6CryptoP256CreateSpanV1,xl6CryptoP256DestroyV1,publicView,xl6KeySec1ReadV1
};
const XL6_KeyExportV1 xl6CryptoP256ExportSpecV1={
    XL6_KEY_P256_V1,privateView,publicView,xl6KeySec1WriteV1
};
