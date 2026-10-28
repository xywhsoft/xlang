#include "xl6_http_internal.h"
#include "xl6_net_buffer_internal.h"
typedef xhttp1status (*Parse)(xnetbuf*,xhttp1head*,const xhttp1limits*,xhttp1errorinfo*);
static void* parse(void* raw,const xhttp1limits* limits,Parse parser)
{
    XL6_NetBufferV1* buffer=raw;XL6_NetBufferGuardV1 guard={0};
    if(!xl6NetBufferEnterV1(buffer,&guard))return NULL;
    xhttpfield fields[100];xhttpfield* extra=NULL;xhttp1head head;xhttp1errorinfo error={0};
    xrtHttp1HeadInit(&head,fields,100);xhttp1status status=parser(&buffer->native,&head,limits,&error);
    if(status==XHTTP1_FIELDS){
        size_t count=head.FieldCount;
        if(count>SIZE_MAX/sizeof(xhttpfield))xrtSetErrorKind(XERR_RANGE);
        else extra=xrtMalloc(count*sizeof(*extra));
        if(extra){xrtHttp1HeadInit(&head,extra,count);status=parser(&buffer->native,&head,limits,&error);}
    }
    XL6_HttpHead* result=NULL;
    if(status==XHTTP1_READY){xnetspan span={0};
        if(!xrtNetBufFront(&buffer->native,&span)||span.Size<head.Bytes)xrtSetErrorKind(XERR_INTERNAL);
        else result=xl6HttpHeadCopy(&head,(xbytesview){span.Data,span.Size});
    }else if(status==XHTTP1_ERROR&&error.Code)xl6HttpParseError(&error);
    xrtFree(extra);xl6NetBufferLeaveV1(&guard);return result;
}
void* xl6Http1HeadRequestNetBufferV2(void* input,int64_t maxHead,int64_t start,int64_t line,int64_t count)
{xhttp1limits limits;return xl6HttpHeadLimits(&limits,maxHead,start,line,count)?parse(input,&limits,xrtHttp1RequestParseBuffer):NULL;}
void* xl6Http1HeadResponseNetBufferV2(void* input,int64_t maxHead,int64_t start,int64_t line,int64_t count)
{xhttp1limits limits;return xl6HttpHeadLimits(&limits,maxHead,start,line,count)?parse(input,&limits,xrtHttp1ResponseParseBuffer):NULL;}
