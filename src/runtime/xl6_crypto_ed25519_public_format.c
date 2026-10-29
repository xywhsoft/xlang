#include "xl6_key_object_internal.h"
#include "xl6_key_abi.h"

xvalue* xl6CryptoEd25519PublicFromSpkiV1(const xvalue* input)
{return xl6KeyPublicFromSpkiV1(input,XL6_KEY_ED25519_V1);}
xvalue* xl6CryptoEd25519PublicToSpkiV1(const xvalue* input)
{return xl6KeyPublicToSpkiV1(input,XL6_KEY_ED25519_V1);}
