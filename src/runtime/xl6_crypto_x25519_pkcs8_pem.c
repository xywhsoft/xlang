#include "xl6_key_curve_codecs.h"

void* xl6CryptoX25519FromPkcs8PemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"PRIVATE KEY",xl6CryptoX25519FromPkcs8SpanV1,xl6CryptoX25519DestroyV1);}
xstrview xl6CryptoX25519ToPkcs8PemV1(void* raw,bool includePublic)
{return xl6KeyObjectToPemV1(raw,includePublic,"PRIVATE KEY",xl6CryptoX25519WritePkcs8SpanV1);}
