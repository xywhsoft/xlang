#include "xl6_crypto_ed25519_internal.h"
#include "xl6_crypto_abi.h"

xvalue* xl6CryptoEd25519PublicFromSeedV1(const xvalue* value)
{
    xbytesview seed;
    if (!xl6CryptoEd25519Material(value,XRT_ED25519_SEED_SIZE,&seed)) return NULL;
    void* output=xrtMalloc(XRT_ED25519_PUBLIC_SIZE);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,XRT_ED25519_PUBLIC_SIZE,xrtEd25519Public(seed.Data,output));
}
