#include "xl6_key_curve_codecs.h"

void* xl6CryptoP384FromPkcs8PemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"PRIVATE KEY",xl6CryptoP384FromPkcs8SpanV1,xl6CryptoP384DestroyV1);}
xstrview xl6CryptoP384ToPkcs8PemV1(void* raw,bool includePublic)
{return xl6KeyObjectToPemV1(raw,includePublic,"PRIVATE KEY",xl6CryptoP384WritePkcs8SpanV1);}
