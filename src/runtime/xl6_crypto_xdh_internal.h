#ifndef XL6_CRYPTO_XDH_INTERNAL_H
#define XL6_CRYPTO_XDH_INTERNAL_H
#include "xl6_crypto_internal.h"

typedef struct XL6_CryptoX25519KeyV1 {
    uint8 privateKey[32];
    uint8 publicKey[32];
} XL6_CryptoX25519KeyV1;
typedef struct XL6_CryptoX448KeyV1 {
    uint8 privateKey[56];
    uint8 publicKey[56];
} XL6_CryptoX448KeyV1;

/* Key material and encoded public points are whole, fixed-size byte strings.
 * Message prefix selection is deliberately not available for these inputs. */
static inline bool xl6CryptoXdhMaterial(const xvalue* value,size_t size,xbytesview* view)
{
    if (!xl6CryptoMaterial(value,view)) return false;
    if (view->Size!=size) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return true;
}
#endif
