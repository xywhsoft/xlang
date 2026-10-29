#include "xl6_key_curve_codecs.h"

void* xl6CryptoX448FromPkcs8PemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"PRIVATE KEY",xl6CryptoX448FromPkcs8SpanV1,xl6CryptoX448DestroyV1);}
xstrview xl6CryptoX448ToPkcs8PemV1(void* raw,bool includePublic)
{return xl6KeyObjectToPemV1(raw,includePublic,"PRIVATE KEY",xl6CryptoX448WritePkcs8SpanV1);}
