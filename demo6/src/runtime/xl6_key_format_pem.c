#include "xl6_key_format_internal.h"

static bool whitespace(xstrview text)
{
    for (size_t i=0;i<text.Size;++i) {
        unsigned char byte=(unsigned char)text.Data[i];
        if (byte!=' ' && byte!='\t' && byte!='\r' && byte!='\n') return false;
    }
    return true;
}
bool xl6KeyPemDecodeV1(xstrview text,const char* label,bytes* data,size_t* size)
{
    if (!data || !size || !label || (text.Size && !text.Data))
        return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"invalid PEM input or outputs");
    if (text.Size>XL6_KEY_TEXT_LIMIT_V1)
        return xl6KeyFormatFailV1(XERR_RANGE,XL6_KEY_FORMAT_LIMIT_V1,"key PEM exceeds input budget");
    xpemcursor cursor; xpemblock block;
    if (!xrtPemInit(&cursor,text.Data,text.Size) || xrtPemRead(&cursor,&block)!=XPEM_BLOCK) {
        if (!xrtGetError()) xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_SCHEMA_V1,"missing PEM key block");
        return false;
    }
    size_t before=(size_t)(block.Raw.Data-text.Data);
    if (before>text.Size || block.Raw.Size>text.Size-before)
        return xl6KeyFormatFailV1(XERR_STATE,XL6_KEY_FORMAT_SCHEMA_V1,"invalid native PEM block view");
    size_t after=before+block.Raw.Size;
    if (block.Label.Size!=strlen(label) || memcmp(block.Label.Data,label,block.Label.Size) ||
        !whitespace((xstrview){text.Data,before}) || !whitespace((xstrview){text.Data+after,text.Size-after}))
        return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_SCHEMA_V1,"expected exactly one PEM key with the requested label");
    size_t count;
    if (!xrtPemDecode(&block,NULL,0,&count)) return false;
    if (count>XL6_KEY_DER_LIMIT_V1)
        return xl6KeyFormatFailV1(XERR_RANGE,XL6_KEY_FORMAT_LIMIT_V1,"decoded key exceeds input budget");
    bytes result=xrtMalloc(count?count:1);
    if (!result) return false;
    size_t written;
    if (!xrtPemDecode(&block,result,count,&written) || written!=count) {
        xl6KeyFormatFailV1(XERR_STATE,XL6_KEY_FORMAT_SCHEMA_V1,"native PEM decoded length changed");
        xl6KeyFreeV1(result,count?count:1);return false;
    }
    *data=result;*size=count;return true;
}
bool xl6KeyPemEncodeV1(xbytesview der,const char* label,str* text,size_t* size)
{
    if (!text || !size || !label)
        return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing PEM output or label");
    size_t count;
    if (!xl6KeyInputV1(der) || !xrtPemEncode(label,der.Data,der.Size,NULL,0,&count)) return false;
    if (count>=SIZE_MAX || count>XL6_KEY_TEXT_LIMIT_V1)
        return xl6KeyFormatFailV1(XERR_RANGE,XL6_KEY_FORMAT_LIMIT_V1,"encoded key exceeds output budget");
    str result=xrtMalloc(count+1);
    if (!result) return false;
    size_t written;
    if (!xrtPemEncode(label,der.Data,der.Size,result,count+1,&written) || written!=count) {
        xl6KeyFormatFailV1(XERR_STATE,XL6_KEY_FORMAT_SCHEMA_V1,"native PEM encoded length changed");
        xl6KeyFreeV1(result,count+1);return false;
    }
    *text=result;*size=count;return true;
}
