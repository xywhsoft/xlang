#include "xl6_x509_internal.h"

static bool whitespace(xstrview text)
{
    for(size_t i=0;i<text.Size;++i) {
        unsigned char c=(unsigned char)text.Data[i];
        if(c!=' ' && c!='\t' && c!='\r' && c!='\n')return false;
    }
    return true;
}
bool xl6X509PemDecodeV1(xstrview text,const char* label,int64_t textLimit,int64_t derLimit,bytes* data,size_t* size)
{
    if(!data || !size || !label || (text.Size && !text.Data))
        return xl6X509FailV1(XERR_ARGUMENT,"invalid X.509 PEM input or outputs");
    if(!xl6X509LimitV1(text.Size,textLimit,"X.509 PEM exceeds input budget") ||
        !xl6X509LimitV1(0,derLimit,"negative X.509 DER budget"))return false;
    xpemcursor cursor;xpemblock block;
    if(!xl6X509OutcomeV1(xrtPemInit(&cursor,text.Data,text.Size)))return false;
    xpemresult status=xrtPemRead(&cursor,&block);
    if(status!=XPEM_BLOCK)return xl6X509FailV1(XERR_VALUE,"missing X.509 PEM block");
    size_t before=(size_t)(block.Raw.Data-text.Data);
    if(before>text.Size || block.Raw.Size>text.Size-before)
        return xl6X509FailV1(XERR_STATE,"invalid native PEM block extent");
    size_t after=before+block.Raw.Size;
    if(block.Label.Size!=strlen(label) || memcmp(block.Label.Data,label,block.Label.Size) ||
        !whitespace((xstrview){text.Data,before}) || !whitespace((xstrview){text.Data+after,text.Size-after}))
        return xl6X509FailV1(XERR_VALUE,"expected exactly one X.509 PEM block with the requested label");
    size_t count;
    if(!xl6X509OutcomeV1(xrtPemDecode(&block,NULL,0,&count)))return false;
    if(count>INT64_MAX || !xl6X509LimitV1(count,derLimit,"decoded X.509 DER exceeds input budget")) {
        xl6X509FailV1(XERR_RANGE,"decoded X.509 DER is too large");return false;
    }
    bytes result=xrtMalloc(count?count:1);if(!result)return false;
    size_t written=0;
    if(!xl6X509OutcomeV1(xrtPemDecode(&block,result,count,&written) && written==count)) {
        xl6X509FreeV1(result);return false;
    }
    *data=result;*size=count;return true;
}
xstrview xl6X509PemEncodeV1(xbytesview input,const char* label,int64_t textLimit)
{
    const xstrview failure={0};size_t count;
    if(!label){xl6X509FailV1(XERR_ARGUMENT,"missing X.509 PEM label");return failure;}
    if(!xl6X509OutcomeV1(xl6X509SpanV1(input) && xrtPemEncode(label,input.Data,input.Size,NULL,0,&count)))return failure;
    if(count>INT64_MAX || count==SIZE_MAX || !xl6X509LimitV1(count,textLimit,"encoded X.509 PEM exceeds output budget")) {
        xl6X509FailV1(XERR_RANGE,"encoded X.509 PEM exceeds output budget");return failure;
    }
    str result=xrtMalloc(count+1);if(!result)return failure;
    size_t written=0;
    if(!xl6X509OutcomeV1(xrtPemEncode(label,input.Data,input.Size,result,count+1,&written) && written==count)) {
        xl6X509FreeV1(result);return failure;
    }
    return (xstrview){result,count};
}
