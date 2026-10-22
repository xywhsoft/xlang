#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"

/* Encoding is bounded by the two supported scalar widths. No curve math,
 * managed key, signing, verification or RNG is needed for this conversion. */
static xvalue* encode(const xvalue* value,size_t scalar)
{
    xbytesview raw;if (!xl6CryptoNistMaterial(value,scalar*2,&raw)) return NULL;
    uint8 output[104]={0};size_t size=0;
    bool ok=xrtEcdsaDerEncode(raw.Data,scalar,output,sizeof(output),&size);
    xvalue* result=ok?xl6CryptoCopySecret(output,size):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
static xvalue* decode(const xvalue* value,size_t scalar)
{
    xbytesview der;if (!xl6CryptoMaterial(value,&der)) return NULL;
    void* output=xrtMalloc(scalar*2);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,scalar*2,xrtEcdsaDerDecode(der.Data,der.Size,output,scalar));
}
xvalue* xl6CryptoEcdsaP256ToDerV1(const xvalue* value) {return encode(value,32);}
xvalue* xl6CryptoEcdsaP256FromDerV1(const xvalue* value) {return decode(value,32);}
xvalue* xl6CryptoEcdsaP384ToDerV1(const xvalue* value) {return encode(value,48);}
xvalue* xl6CryptoEcdsaP384FromDerV1(const xvalue* value) {return decode(value,48);}
