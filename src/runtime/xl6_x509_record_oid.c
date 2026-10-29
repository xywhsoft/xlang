#include "xl6_x509_records_internal.h"

bool xl6X509OidInputV1(const xvalue* value,xbytesview* input)
{
    if(!xl6X509OutcomeV1(xrtValueGetBytes(value,input)))return false;
    if(!input->Data || !input->Size)return xl6X509FailV1(XERR_ARGUMENT,"empty X.509 OID content");
    return true;
}
bool xl6X509OidEncodeV1(xstrview text,xbuffer* output)
{return xl6X509OutcomeV1(xrtDerOidEncode(text,output));}
xstrview xl6X509OidTextV1(xbytesview oid)
{
    const xstrview failure={0};xbuffer text={0};
    bool ok=xl6X509OutcomeV1(xrtDerOidDecode(oid,&text));
    if(ok && text.Size>=INT64_MAX)ok=xl6X509FailV1(XERR_RANGE,"OID text is too large");
    if(ok)ok=xl6X509OutcomeV1(xrtBufferAppend(&text,(xbytesview){(const uint8*)"",1}));
    size_t size=0;bytes data=ok?xrtBufferTake(&text,&size,NULL):NULL;
    xl6X509BufferFreeV1(&text);
    if(!xl6X509OutcomeV1(data!=NULL && size!=0)){xl6X509FreeV1(data);return failure;}
    return (xstrview){(const char*)data,size-1};
}
