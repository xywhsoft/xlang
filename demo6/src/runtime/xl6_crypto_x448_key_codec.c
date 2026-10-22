#include "xl6_key_curve_codecs.h"
#include "xl6_crypto_xdh_internal.h"

static xbytesview privateView(const void* raw)
{
    const XL6_CryptoX448KeyV1* self=raw;
    return (xbytesview){self->privateKey,sizeof(self->privateKey)};
}
static xbytesview publicView(const void* raw)
{
    const XL6_CryptoX448KeyV1* self=raw;
    return (xbytesview){self->publicKey,sizeof(self->publicKey)};
}
const XL6_KeyImportV1 xl6CryptoX448ImportSpecV1={
    XL6_KEY_X448_V1,xl6CryptoX448CreateSpanV1,xl6CryptoX448DestroyV1,publicView,xl6KeyObjectCurveDecodeV1
};
const XL6_KeyExportV1 xl6CryptoX448ExportSpecV1={
    XL6_KEY_X448_V1,privateView,publicView,xl6KeyObjectCurveEncodeV1
};
