#include "xl6_key_curve_codecs.h"

void* xl6CryptoP384FromPkcs8SpanV1(xbytesview input)
{return xl6KeyObjectFromPkcs8V1(&xl6CryptoP384ImportSpecV1,input);}
void* xl6CryptoP384FromPkcs8V1(const xvalue* value)
{
    xbytesview input;
    return xl6KeyObjectInputV1(value,&input)?xl6CryptoP384FromPkcs8SpanV1(input):NULL;
}
bool xl6CryptoP384WritePkcs8SpanV1(void* raw,bool includePublic,xbuffer* output)
{return xl6KeyObjectWritePkcs8V1(&xl6CryptoP384ExportSpecV1,raw,includePublic,output);}
xvalue* xl6CryptoP384ToPkcs8V1(void* raw,bool includePublic)
{
    xbuffer output={0};bool ok=xl6CryptoP384WritePkcs8SpanV1(raw,includePublic,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
