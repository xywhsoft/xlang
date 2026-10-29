#include "xl6_key_curve_codecs.h"
void* xl6CryptoRsaPrivateFromPkcs1PemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"RSA PRIVATE KEY",xl6CryptoRsaPrivateFromPkcs1SpanV1,xl6CryptoRsaPrivateDestroyV1);}
xstrview xl6CryptoRsaPrivateToPkcs1PemV1(void* raw)
{return xl6KeyObjectToPemV1(raw,false,"RSA PRIVATE KEY",xl6CryptoRsaPrivateWritePkcs1SpanV1);}
