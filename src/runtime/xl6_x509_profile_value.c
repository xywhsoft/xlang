#include "xl6_x509_profiles_internal.h"

bool xl6X509ProfileInputV1(const xvalue* value,int64_t limit,xbytesview* input)
{
    return xl6X509InputV1(value,input) && xl6X509LimitV1(input->Size,limit,"X.509 profile DER exceeds input budget");
}
bool xl6X509SequenceContentV1(xbytesview raw,xbytesview* content)
{
    xdercursor cursor;xdervalue sequence;
    if(!xl6X509OutcomeV1(xrtDerInit(&cursor,raw.Data,raw.Size) && xrtDerRead(&cursor,&sequence)==XDER_VALUE))return false;
    if(!xrtDerDone(&cursor) || !xrtDerIs(&sequence,XASN1_UNIVERSAL,XASN1_SEQUENCE,true))
        return xl6X509FailV1(XERR_PROTOCOL,"expected one complete X.509 DER sequence");
    *content=sequence.Value;return true;
}
xbytesview xl6X509CursorContentV1(xdercursor cursor)
{
    if(cursor.Offset || (cursor.Size && !cursor.Data)) {
        xl6X509FailV1(XERR_STATE,"expected an initial X.509 snapshot cursor");return (xbytesview){0};
    }
    return (xbytesview){cursor.Data,cursor.Size};
}
xvalue* xl6X509EncodeContentV1(uint32 tag,xbytesview content)
{
    xbuffer encoded={0};bytes owned=NULL;size_t size=0;xvalue* result=NULL;
    bool ok=xl6X509OutcomeV1(xrtDerAppend(&encoded,XASN1_UNIVERSAL,tag,true,content));
    if(ok) {
        owned=xrtBufferTake(&encoded,&size,NULL);
        if(xl6X509OutcomeV1(owned!=NULL))result=xrtValueBytesTake(&owned,size);
    }
    xl6X509FreeV1(owned);xl6X509BufferFreeV1(&encoded);
    if(xl6X509OutcomeV1(result!=NULL))return result;
    xerror* prior=xrtTakeError();xrtValueRelease(result);xrtClearError();xrtSetErrorTake(prior);return NULL;
}
xstrview xl6X509ProfileTextV1(xbytesview input)
{
    if((input.Size && !input.Data) || input.Size>=INT64_MAX || input.Size==SIZE_MAX) {
        xl6X509FailV1(XERR_STATE,"invalid X.509 IA5 text view");return (xstrview){0};
    }
    char* result=xrtMalloc(input.Size+1);if(!result)return (xstrview){0};
    if(input.Size)memcpy(result,input.Data,input.Size);
    result[input.Size]=0;
    return (xstrview){result,input.Size};
}
bool xl6X509ProfileExtensionV1(const xx509cert* certificate,uint8 last,xbytesview* raw)
{
    /* The typed XRT accessor has already checked presence and policy. This
     * public lookup obtains its complete owned source, never pointer-guessing
     * a containing TLV from interior fields. No missing-extension error path. */
    const uint8 oid[]={0x55,0x1d,last};xx509ext extension;
    if(!xl6X509OutcomeV1(xrtX509ExtensionFind(certificate,oid,sizeof(oid),&extension)))return false;
    *raw=extension.Value;return true;
}
const xx509cert* xl6X509ProfileCertificateV1(void* raw)
{
    return xl6X509ProfileCheckedV1(raw)?&((XL6_X509CertificateV1*)raw)->view:NULL;
}
