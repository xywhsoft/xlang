#include "xl6_key_curve_codecs.h"
#include "xl6_crypto_ed25519_internal.h"

static xbytesview privateView(const void* raw)
{
    const XL6_CryptoEd25519KeyV1* self=raw;
    return (xbytesview){self->seed,sizeof(self->seed)};
}
static xbytesview publicView(const void* raw)
{
    const XL6_CryptoEd25519KeyV1* self=raw;
    return (xbytesview){self->expanded.Public,sizeof(self->expanded.Public)};
}
const XL6_KeyImportV1 xl6CryptoEd25519ImportSpecV1={
    XL6_KEY_ED25519_V1,xl6CryptoEd25519CreateSpanV1,xl6CryptoEd25519DestroyV1,publicView,xl6KeyObjectCurveDecodeV1
};
const XL6_KeyExportV1 xl6CryptoEd25519ExportSpecV1={
    XL6_KEY_ED25519_V1,privateView,publicView,xl6KeyObjectCurveEncodeV1
};
