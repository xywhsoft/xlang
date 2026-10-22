#include "xl6_key_curve_codecs.h"

void* xl6CryptoP256FromPkcs8PemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"PRIVATE KEY",xl6CryptoP256FromPkcs8SpanV1,xl6CryptoP256DestroyV1);}
xstrview xl6CryptoP256ToPkcs8PemV1(void* raw,bool includePublic)
{return xl6KeyObjectToPemV1(raw,includePublic,"PRIVATE KEY",xl6CryptoP256WritePkcs8SpanV1);}
