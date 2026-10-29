#include "xl6_crypto_nist_verify_internal.h"
#include "xl6_crypto_abi.h"
bool xl6CryptoP256VerifyHashV1(const xvalue* publicValue,const xvalue* digestValue,const xvalue* signatureValue)
{
    xbytesview key,digest,signature;
    if (!xl6CryptoNistMaterial(publicValue,65,&key) ||
        !xl6CryptoNistDigest(digestValue,&digest) ||
        !xl6CryptoNistMaterial(signatureValue,64,&signature)) return false;
    xerror* previous=xrtTakeError();
    bool ok=xrtEcdsaP256Verify(digest.Data,digest.Size,signature.Data,key.Data);
    return xl6CryptoNistVerifyFinish(ok,previous);
}
