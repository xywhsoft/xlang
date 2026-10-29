#include "xl6_key_curve_codecs.h"
void* xl6CryptoRsaPrivateFromPkcs8PemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"PRIVATE KEY",xl6CryptoRsaPrivateFromPkcs8SpanV1,xl6CryptoRsaPrivateDestroyV1);}
xstrview xl6CryptoRsaPrivateToPkcs8PemV1(void* raw,bool includePublic)
{return xl6KeyObjectToPemV1(raw,includePublic,"PRIVATE KEY",xl6CryptoRsaPrivateWritePkcs8SpanV1);}
