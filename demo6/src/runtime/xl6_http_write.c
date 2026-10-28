#include "xl6_http_internal.h"
bool xl6Http1TargetValidV2(xstrview target,int64_t size)
{return xl6TextSelectRaw(&target,size)&&xrtHttp1TargetValid(target);}
static bool outputSpace(xbuffer* output,size_t size)
{if(size>(size_t)INT64_MAX-output->Size){xrtSetErrorKind(XERR_RANGE);return false;}return xl6BufferAllocation(xrtBufferReserve(output,output->Size+size));}
int64_t xl6Http1WriteRequestHeadV2(xstrview method,xstrview target,void* headers,void* raw,int64_t version)
{
    if(version!=10&&version!=11){xrtSetErrorKind(XERR_RANGE);return 0;}
    xbuffer* output=xl6BufferChecked(raw);XL6_HttpGuard guard={0};const xhttpfield* f;size_t n,size=0;
    if(!output||!xl6HttpHeadersBorrow(headers,&guard,&f,&n))return 0;
    bool ok=xrtHttp1RequestWrite(method,target,(xhttpversion)version,f,n,NULL,0,&size)&&outputSpace(output,size);
    if(ok)ok=xrtHttp1RequestWrite(method,target,(xhttpversion)version,f,n,output->Data+output->Size,size,&size);
    if(ok)output->Size+=size;
    xl6HttpLeave(&guard);return ok?(int64_t)size:0;
}
int64_t xl6Http1WriteResponseHeadV2(int64_t status,xstrview reason,void* headers,void* raw,int64_t version)
{
    uint16 code;if(!xl6HttpU16(status,&code))return 0;
    if(version!=10&&version!=11){xrtSetErrorKind(XERR_RANGE);return 0;}
    xbuffer* output=xl6BufferChecked(raw);XL6_HttpGuard guard={0};const xhttpfield* f;size_t n,size=0;
    if(!output||!xl6HttpHeadersBorrow(headers,&guard,&f,&n))return 0;
    bool ok=xrtHttp1ResponseWrite((xhttpversion)version,code,reason,f,n,NULL,0,&size)&&outputSpace(output,size);
    if(ok)ok=xrtHttp1ResponseWrite((xhttpversion)version,code,reason,f,n,output->Data+output->Size,size,&size);
    if(ok)output->Size+=size;
    xl6HttpLeave(&guard);return ok?(int64_t)size:0;
}
xvalue* xl6Http1RequestHeadV2(xstrview method,xstrview target,void* headers,int64_t version)
{xbuffer out={0};xl6Http1WriteRequestHeadV2(method,target,headers,&out,version);xvalue* value=xrtGetError()?NULL:xl6HttpTakeBuffer(&out);xrtBufferUnit(&out);return value;}
xvalue* xl6Http1ResponseHeadV2(int64_t status,xstrview reason,void* headers,int64_t version)
{xbuffer out={0};xl6Http1WriteResponseHeadV2(status,reason,headers,&out,version);xvalue* value=xrtGetError()?NULL:xl6HttpTakeBuffer(&out);xrtBufferUnit(&out);return value;}
