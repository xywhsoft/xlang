#include "xl6_http_internal.h"
typedef struct XL6_HttpMessage {xhttp1message value;xhttpfield fields[];} XL6_HttpMessage;
static void* copy(const xhttp1message* message)
{
    size_t count=message->Head.FieldCount,total=sizeof(XL6_HttpMessage);
    if(!xl6HttpAccumulate(&count,message->TrailerCount)||count>SIZE_MAX/sizeof(xhttpfield)||
       !xl6HttpAccumulate(&total,count*sizeof(xhttpfield))||!xl6HttpAccumulate(&total,message->Wire.Size))return NULL;
    XL6_HttpMessage* self=xrtMalloc(total);if(!self)return NULL;self->value=*message;
    char* data=(char*)(self->fields+count);if(message->Wire.Size)memcpy(data,message->Wire.Data,message->Wire.Size);
    self->value.Wire=(xbytesview){(const uint8*)data,message->Wire.Size};
    self->value.Head.Fields=self->fields;self->value.Head.FieldCapacity=message->Head.FieldCount;
    self->value.Trailers=self->fields+message->Head.FieldCount;self->value.TrailerCapacity=message->TrailerCount;
    const uint8* old=message->Wire.Data;
    self->value.Head.Method=xl6HttpRebase(message->Head.Method,old,data);
    self->value.Head.Target=xl6HttpRebase(message->Head.Target,old,data);
    self->value.Head.Reason=xl6HttpRebase(message->Head.Reason,old,data);
    for(size_t i=0;i<message->Head.FieldCount;++i)self->fields[i]=(xhttpfield){
        xl6HttpRebase(message->Head.Fields[i].Name,old,data),xl6HttpRebase(message->Head.Fields[i].Value,old,data)};
    for(size_t i=0;i<message->TrailerCount;++i)self->value.Trailers[i]=(xhttpfield){
        xl6HttpRebase(message->Trailers[i].Name,old,data),xl6HttpRebase(message->Trailers[i].Value,old,data)};
    return self;
}
static void* parse(xbytesview input,bool end,xstrview method,bool response,const xhttp1limits* head,const xhttp1bodylimits* body)
{
    xhttpfield fields[100],trailers[32];xhttpfield *extraHead=NULL,*extraTrailers=NULL;
    size_t headCapacity=100,trailerCapacity=32;xhttp1message message;xhttp1errorinfo error={0};
    xrtHttp1MessageInit(&message,fields,headCapacity,trailers,trailerCapacity);
    xhttp1status status;
    for(;;){
        status=response?xrtHttp1ResponseMessageParse(input,end,method,&message,head,body,&error):
            xrtHttp1RequestMessageParse(input,end,&message,head,body,&error);
        if(status!=XHTTP1_FIELDS)break;
        bool growHead=message.Head.FieldCount>headCapacity,growTrailer=message.TrailerCount>trailerCapacity;
        if((!growHead&&!growTrailer)||message.Head.FieldCount>SIZE_MAX/sizeof(xhttpfield)||message.TrailerCount>SIZE_MAX/sizeof(xhttpfield)){
            xrtSetErrorKind(XERR_INTERNAL);break;
        }
        if(growHead){headCapacity=message.Head.FieldCount;xrtFree(extraHead);extraHead=xrtMalloc(headCapacity*sizeof(xhttpfield));if(!extraHead)break;}
        if(growTrailer){trailerCapacity=message.TrailerCount;xrtFree(extraTrailers);extraTrailers=xrtMalloc(trailerCapacity*sizeof(xhttpfield));if(!extraTrailers)break;}
        xrtHttp1MessageInit(&message,extraHead?extraHead:fields,headCapacity,extraTrailers?extraTrailers:trailers,trailerCapacity);
    }
    void* result=status==XHTTP1_READY?copy(&message):NULL;
    if(status==XHTTP1_ERROR)xl6HttpParseError(&error);
    xrtFree(extraHead);xrtFree(extraTrailers);return result;
}
#define LIMIT_ARGS int64_t maxHead,int64_t start,int64_t line,int64_t fields,int64_t maxBody,int64_t chunk,int64_t trailer,int64_t trailerLine,int64_t trailerFields,int64_t size
#define LIMIT_VALUES maxHead,start,line,fields,maxBody,chunk,trailer,trailerLine,trailerFields
#define PREPARE xhttp1limits head;xhttp1bodylimits body;if(!xl6BufferSelect(&input,size)||!xl6HttpHeadLimits(&head,maxHead,start,line,fields)||!xl6HttpBodyLimits(&body,maxBody,chunk,trailer,trailerLine,trailerFields))return NULL
void* xl6Http1MessageRequestSpanV2(xbytesview input,bool end,LIMIT_ARGS)
{PREPARE;return parse(input,end,(xstrview){NULL,0},false,&head,&body);}
void* xl6Http1MessageResponseSpanV2(xbytesview input,bool end,xstrview method,LIMIT_ARGS)
{PREPARE;return parse(input,end,method,true,&head,&body);}
#define WRAPPERS(Suffix,Type,Input) \
void* xl6Http1MessageRequest##Suffix##V2(Type raw,bool end,LIMIT_ARGS) \
{xbytesview input;return Input(raw,size,&input)?xl6Http1MessageRequestSpanV2(input,end,LIMIT_VALUES,-1):NULL;} \
void* xl6Http1MessageResponse##Suffix##V2(Type raw,bool end,xstrview method,LIMIT_ARGS) \
{xbytesview input;return Input(raw,size,&input)?xl6Http1MessageResponseSpanV2(input,end,method,LIMIT_VALUES,-1):NULL;}
WRAPPERS(Bytes,xvalue*,xl6BufferBytesInput)
WRAPPERS(Buffer,void*,xl6BufferInput)
#undef WRAPPERS
#undef PREPARE
#undef LIMIT_ARGS
#undef LIMIT_VALUES
void xl6Http1MessageDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6Http1MessageWireBytesV2(void* raw){return (int64_t)((XL6_HttpMessage*)raw)->value.Wire.Size;}
int64_t xl6Http1MessageBodyBytesV2(void* raw){return (int64_t)((XL6_HttpMessage*)raw)->value.BodyBytes;}
void* xl6Http1MessageHeadV2(void* raw)
{XL6_HttpMessage* self=raw;return xl6HttpHeadCopy(&self->value.Head,self->value.Wire);}
void* xl6Http1MessageBodyPlanV2(void* raw)
{XL6_HttpMessage* self=raw;return xl6HttpBodyPlanCreateV2(self->value.Plan.Mode,self->value.Plan.Length);}
void* xl6Http1MessageTrailersV2(void* raw)
{XL6_HttpMessage* self=raw;return xl6HttpHeadersCopyFields(self->value.Trailers,self->value.TrailerCount);}
int64_t xl6Http1MessageCopyBodyIntoV2(void* raw,void* target)
{
    XL6_HttpMessage* self=raw;xbuffer* out=xl6BufferChecked(target);size_t size=0;
    if(!out||!xrtHttp1MessageBodyCopy(&self->value,NULL,0,&size))return 0;
    if(size>(size_t)INT64_MAX-out->Size){xrtSetErrorKind(XERR_RANGE);return 0;}
    if(!xl6BufferAllocation(xrtBufferReserve(out,out->Size+size))||
       !xrtHttp1MessageBodyCopy(&self->value,out->Data?out->Data+out->Size:NULL,size,&size))return 0;
    out->Size+=size;return (int64_t)size;
}
xvalue* xl6Http1MessageBodyV2(void* raw)
{xbuffer out={0};xl6Http1MessageCopyBodyIntoV2(raw,&out);xvalue* value=xrtGetError()?NULL:xl6HttpTakeBuffer(&out);xrtBufferUnit(&out);return value;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6Http1MessageHandlePolicyV2Get,xl6Http1MessageDestroyV2)
