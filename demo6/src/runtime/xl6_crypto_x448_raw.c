#include "xl6_crypto_xdh_internal.h"
#include "xl6_crypto_abi.h"

/* Explicit advanced RFC 7748 primitive, not a shared-secret validation API.
 * This preserves XRT scalar/u-coordinate decoding, including its output rule. */
xvalue* xl6CryptoX448RawV1(const xvalue* scalarValue,const xvalue* pointValue)
{
    xbytesview scalar,point;
    if (!xl6CryptoXdhMaterial(scalarValue,56,&scalar) ||
        !xl6CryptoXdhMaterial(pointValue,56,&point)) return NULL;
    void* output=xrtMalloc(56);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,56,xrtX448(scalar.Data,point.Data,output));
}
