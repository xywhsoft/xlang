#ifndef XL6_CRYPTO_ED25519_INTERNAL_H
#define XL6_CRYPTO_ED25519_INTERNAL_H
#include "xl6_crypto_internal.h"

typedef struct XL6_CryptoEd25519KeyV1 {
    uint8 seed[XRT_ED25519_SEED_SIZE];
    xed25519key expanded;
} XL6_CryptoEd25519KeyV1;

static inline bool xl6CryptoEd25519Material(const xvalue* value,size_t size,xbytesview* view)
{
    if (!xl6CryptoMaterial(value,view)) return false;
    if (view->Size!=size) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return true;
}
static inline bool xl6CryptoEd25519Context(const xvalue* value,xbytesview* view)
{
    if (!xl6CryptoMaterial(value,view)) return false;
    if (view->Size>XRT_ED25519_CONTEXT_MAX_SIZE) {xrtSetErrorKind(XERR_RANGE);return false;}
    return true;
}
/* Resident-to-resident spans; never language-visible descriptors or callbacks. */
XL6_TEXT_API xvalue* xl6CryptoEd25519KeySignSpanV1(void*,xbytesview,xed25519mode,const xvalue*);
XL6_TEXT_API xvalue* xl6CryptoEd25519SeedSignSpanV1(const xvalue*,xbytesview);
XL6_TEXT_API bool xl6CryptoEd25519VerifySpanV1(const xvalue*,xbytesview,const xvalue*,xed25519mode,const xvalue*);
#endif
