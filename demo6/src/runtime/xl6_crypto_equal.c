#include "xl6_crypto_internal.h"
bool xl6CryptoTimingSafeEqualV1(const xvalue* left,const xvalue* right)
{
    xbytesview a,b;
    if (!xl6CryptoMaterial(left,&a) || !xl6CryptoMaterial(right,&b)) return false;
    /* Length is public; equal-length contents use the audited XRT primitive. */
    return a.Size==b.Size && xrtConstTimeEqual(a.Data,b.Data,a.Size);
}
