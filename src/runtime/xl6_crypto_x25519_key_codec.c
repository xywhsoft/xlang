#include "xl6_key_curve_codecs.h"
#include "xl6_crypto_xdh_internal.h"

static xbytesview privateView(const void* raw)
{
    const XL6_CryptoX25519KeyV1* self=raw;
    return (xbytesview){self->privateKey,sizeof(self->privateKey)};
}
static xbytesview publicView(const void* raw)
{
    const XL6_CryptoX25519KeyV1* self=raw;
    return (xbytesview){self->publicKey,sizeof(self->publicKey)};
}
const XL6_KeyImportV1 xl6CryptoX25519ImportSpecV1={
    XL6_KEY_X25519_V1,xl6CryptoX25519CreateSpanV1,xl6CryptoX25519DestroyV1,publicView,xl6KeyObjectCurveDecodeV1
};
const XL6_KeyExportV1 xl6CryptoX25519ExportSpecV1={
    XL6_KEY_X25519_V1,privateView,publicView,xl6KeyObjectCurveEncodeV1
};
