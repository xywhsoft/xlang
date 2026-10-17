#ifndef XL6_CRYPTO_INTERNAL_H
#define XL6_CRYPTO_INTERNAL_H
#include "xl6_buffer_internal.h"

/* Cryptographic material is always consumed in full. Only message data has a
 * clamped prefix limit. Never reuse that limit for a key, nonce or tag. */
static inline bool xl6CryptoMaterial(const xvalue* value,xbytesview* view)
{return xl6BufferBytesInput(value,-1,view);}

static inline void xl6CryptoFreeSecret(void* data,size_t size)
{
    xerror* error=xrtTakeError();
    if (data) xrtSecureZero(data,size);
    xrtFree(data);xrtClearError();xrtSetErrorTake(error);
}
static inline xvalue* xl6CryptoCopySecret(const void* data,size_t size)
{return xrtValueBytes((xbytesview){data,size});}
static inline xvalue* xl6CryptoTakeSecret(void* data,size_t size,bool ok)
{
    /* XRT's take contract clears the slot only on success. A failed value
     * allocation leaves it here for secure rollback; success needs no copy. */
    bytes slot=data;
    xvalue* value=ok?xrtValueBytesTake(&slot,size):NULL;
    xl6CryptoFreeSecret(slot,size);return value;
}
static inline xstrview xl6CryptoHex(const uint8* data,size_t size)
{
    /* Hex formatting is bounded here (digests <= 64 bytes), not another crypto
     * algorithm or a dependency on the complete codec family. */
    static const char digits[]="0123456789abcdef";
    char* result=xrtMalloc(size*2+1);if (!result) return xl6TextOwn(NULL,0);
    for (size_t i=0;i<size;++i) {result[2*i]=digits[data[i]>>4];result[2*i+1]=digits[data[i]&15];}
    result[size*2]=0;return xl6TextOwn(result,size*2);
}
static inline bool xl6CryptoLength(int64_t size,size_t limit,size_t* result)
{
    if (size<0 || (uint64_t)size>limit || (uint64_t)size>SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    *result=(size_t)size;return true;
}
#endif
