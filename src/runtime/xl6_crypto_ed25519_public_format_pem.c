#include "xl6_key_object_internal.h"
#include "xl6_key_abi.h"

xvalue* xl6CryptoEd25519PublicFromSpkiPemV1(xstrview text)
{return xl6KeyPublicFromSpkiPemV1(text,XL6_KEY_ED25519_V1);}
xstrview xl6CryptoEd25519PublicToSpkiPemV1(const xvalue* input)
{return xl6KeyPublicToSpkiPemV1(input,XL6_KEY_ED25519_V1);}
