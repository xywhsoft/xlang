#include "xl6_crypto_xdh_internal.h"
#include "xl6_crypto_abi.h"

/* Explicit advanced RFC 7748 primitive, not a shared-secret validation API.
 * This preserves XRT scalar/u-coordinate decoding, including its output rule. */
xvalue* xl6CryptoX25519RawV1(const xvalue* scalarValue,const xvalue* pointValue)
{
    xbytesview scalar,point;
    if (!xl6CryptoXdhMaterial(scalarValue,32,&scalar) ||
        !xl6CryptoXdhMaterial(pointValue,32,&point)) return NULL;
    void* output=xrtMalloc(32);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,32,xrtX25519(scalar.Data,point.Data,output));
}
