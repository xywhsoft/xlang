#include "xl6_key_object_internal.h"
#include "xl6_key_abi.h"

xvalue* xl6CryptoX448PublicFromSpkiV1(const xvalue* input)
{return xl6KeyPublicFromSpkiV1(input,XL6_KEY_X448_V1);}
xvalue* xl6CryptoX448PublicToSpkiV1(const xvalue* input)
{return xl6KeyPublicToSpkiV1(input,XL6_KEY_X448_V1);}
