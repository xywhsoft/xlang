#include "xl6_http_internal.h"
xstrview xl6HttpStatusTextV2(int64_t status)
{uint16 value;if(!xl6HttpU16(status,&value))return xl6TextFailure;return xl6TextCopy(xrtHttpStatusText(value));}
bool xl6HttpTokenValidV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)&&xrtHttpTokenValid(text);}
bool xl6HttpTokenEqualV2(xstrview left,xstrview right){return xrtHttpTokenEqual(left,right);}
int64_t xl6HttpMethodCodeV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)?(int64_t)xrtHttpMethodParse(text):0;}
bool xl6HttpMethodEqualV2(xstrview left,xstrview right){return xrtHttpMethodEqual(left,right);}
bool xl6HttpMethodSafeV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)&&xrtHttpMethodSafe(text);}
bool xl6HttpMethodIdempotentV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)&&xrtHttpMethodIdempotent(text);}
bool xl6HttpResponseContentAllowedV2(xstrview method,int64_t status)
{uint16 code;return xl6HttpU16(status,&code)&&xrtHttpResponseContentAllowed(method,code);}
xstrview xl6HttpTrimOwsV2(xstrview text,int64_t size)
{return xl6TextSelect(&text,size)?xl6TextCopy(xrtHttpOwsTrim(text)):xl6TextFailure;}
int64_t xl6HttpQualityV2(xstrview text,int64_t size)
{uint16 result=0;if(!xl6TextSelectRaw(&text,size)||!xrtHttpQualityParse(text,&result))return 0;return result;}
uint64_t xl6HttpContentLengthV2(xstrview text,int64_t size)
{uint64 result=0;if(!xl6TextSelectRaw(&text,size)||!xrtHttpContentLengthParse(text,&result))return 0;return result;}
bool xl6HttpFieldValueValidV2(xvalue* data,int64_t size)
{xbytesview view;return xl6BufferBytesInput(data,size,&view)&&xrtHttpFieldValueValid(xl6HttpAsText(view));}
bool xl6HttpFieldNameEqualV2(xstrview left,xstrview right){return xrtHttpFieldNameEqual(left,right);}
bool xl6HttpFieldValueValidTextV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)&&xrtHttpFieldValueValid(text);}
bool xl6HttpFieldValueValidSpanV2(xbytesview input,int64_t size)
{return xl6BufferSelect(&input,size)&&xrtHttpFieldValueValid(xl6HttpAsText(input));}
