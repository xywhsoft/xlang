#include "xl6_crypto_internal.h"
bool xl6CryptoTimingSafeEqualV1(const xvalue* left,const xvalue* right)
{
    xbytesview a,b;
    if (!xl6CryptoMaterial(left,&a) || !xl6CryptoMaterial(right,&b)) return false;
    /* Length is public; equal-length contents use the audited XRT primitive. */
    return a.Size==b.Size && xrtConstTimeEqual(a.Data,b.Data,a.Size);
}

/* Synchronous readonly span: the caller keeps the input owner alive. */
bool xl6CryptoTimingSafeEqualSpanV1(xbytesview left,xbytesview right)
{
    xbytesview a,b;
    if (!xl6BufferSpanInput(left,-1,&a) || !xl6BufferSpanInput(right,-1,&b)) return false;
    /* Length is public; equal-length contents use the audited XRT primitive. */
    return a.Size==b.Size && xrtConstTimeEqual(a.Data,b.Data,a.Size);
}
