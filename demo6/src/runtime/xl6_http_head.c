#include "xl6_http_internal.h"
XL6_HttpHead* xl6HttpHeadCopy(const xhttp1head* value,xbytesview wire)
{
    size_t total=sizeof(XL6_HttpHead);
    if(value->FieldCount>SIZE_MAX/sizeof(xhttpfield)||value->Bytes>wire.Size){xrtSetErrorKind(XERR_RANGE);return NULL;}
    if(!xl6HttpAccumulate(&total,value->FieldCount*sizeof(xhttpfield))||!xl6HttpAccumulate(&total,value->Bytes))return NULL;
    XL6_HttpHead* self=xrtMalloc(total);if(!self)return NULL;
    self->value=*value;self->value.Fields=self->fields;self->value.FieldCapacity=value->FieldCount;
    char* data=(char*)(self->fields+value->FieldCount);if(value->Bytes)memcpy(data,wire.Data,value->Bytes);
    self->value.Method=xl6HttpRebase(value->Method,wire.Data,data);
    self->value.Target=xl6HttpRebase(value->Target,wire.Data,data);
    self->value.Reason=xl6HttpRebase(value->Reason,wire.Data,data);
    for(size_t i=0;i<value->FieldCount;++i)self->fields[i]=(xhttpfield){
        xl6HttpRebase(value->Fields[i].Name,wire.Data,data),xl6HttpRebase(value->Fields[i].Value,wire.Data,data)};
    return self;
}
typedef xhttp1status (*Parse)(xbytesview,xhttp1head*,const xhttp1limits*,xhttp1errorinfo*);
static void* parse(xbytesview input,const xhttp1limits* limits,Parse parser)
{
    xhttpfield local[100];xhttp1head head;xhttp1errorinfo error={0};xhttpfield* extra=NULL;
    xrtHttp1HeadInit(&head,local,100);xhttp1status status=parser(input,&head,limits,&error);
    if(status==XHTTP1_FIELDS){
        if(head.FieldCount>SIZE_MAX/sizeof(xhttpfield)){xrtSetErrorKind(XERR_RANGE);return NULL;}
        size_t capacity=head.FieldCount;extra=xrtMalloc(capacity*sizeof(*extra));if(!extra)return NULL;
        xrtHttp1HeadInit(&head,extra,capacity);status=parser(input,&head,limits,&error);
    }
    XL6_HttpHead* result=status==XHTTP1_READY?xl6HttpHeadCopy(&head,input):NULL;
    if(status==XHTTP1_ERROR)xl6HttpParseError(&error);
    else if(status==XHTTP1_FIELDS)xrtSetErrorKind(XERR_INTERNAL);
    xrtFree(extra);return result;
}
#define FACTORY(Direction,Parser) \
void* xl6Http1Head##Direction##SpanV2(xbytesview input,int64_t maxHead,int64_t start,int64_t line,int64_t fields,int64_t size) \
{xhttp1limits limits;return xl6BufferSelect(&input,size)&&xl6HttpHeadLimits(&limits,maxHead,start,line,fields)?parse(input,&limits,Parser):NULL;} \
void* xl6Http1Head##Direction##TextV2(xstrview input,int64_t maxHead,int64_t start,int64_t line,int64_t fields,int64_t size) \
{return xl6Http1Head##Direction##SpanV2(xl6HttpAsBytes(input),maxHead,start,line,fields,size);} \
void* xl6Http1Head##Direction##BytesV2(xvalue* input,int64_t maxHead,int64_t start,int64_t line,int64_t fields,int64_t size) \
{xbytesview view;return xl6BufferBytesInput(input,size,&view)?xl6Http1Head##Direction##SpanV2(view,maxHead,start,line,fields,-1):NULL;} \
void* xl6Http1Head##Direction##BufferV2(void* input,int64_t maxHead,int64_t start,int64_t line,int64_t fields,int64_t size) \
{xbytesview view;return xl6BufferInput(input,size,&view)?xl6Http1Head##Direction##SpanV2(view,maxHead,start,line,fields,-1):NULL;}
FACTORY(Request,xrtHttp1RequestParse)
FACTORY(Response,xrtHttp1ResponseParse)
#undef FACTORY
void xl6Http1HeadDestroyV2(void* raw){xrtFree(raw);}
#define NUMBER(Name,Field) int64_t xl6Http1Head##Name##V2(void* raw){return (int64_t)((XL6_HttpHead*)raw)->value.Field;}
NUMBER(Kind,Kind) NUMBER(Version,Version) NUMBER(Flags,Flags) NUMBER(StatusCode,Status)
NUMBER(HeaderBytes,Bytes) NUMBER(FieldCount,FieldCount) NUMBER(MethodCode,MethodCode)
#undef NUMBER
uint64_t xl6Http1HeadContentLengthV2(void* raw){return ((XL6_HttpHead*)raw)->value.ContentLength;}
#define TEXT(Name,Field) xstrview xl6Http1Head##Name##V2(void* raw){return xl6HttpText(((XL6_HttpHead*)raw)->value.Field);}
TEXT(Method,Method) TEXT(Target,Target) TEXT(Reason,Reason)
#undef TEXT
void* xl6Http1HeadHeadersV2(void* raw)
{XL6_HttpHead* self=raw;return xl6HttpHeadersCopyFields(self->fields,self->value.FieldCount);}
xstrview xl6Http1HeadFieldNameV2(void* raw,int64_t index)
{XL6_HttpHead* self=raw;return xl6HttpIndex(index,self->value.FieldCount)?xl6TextCopy(self->fields[index].Name):xl6TextFailure;}
xvalue* xl6Http1HeadFieldValueV2(void* raw,int64_t index)
{XL6_HttpHead* self=raw;return xl6HttpIndex(index,self->value.FieldCount)?xl6HttpBytes(xl6HttpAsBytes(self->fields[index].Value)):NULL;}
xstrview xl6Http1HeadFieldTextV2(void* raw,int64_t index)
{XL6_HttpHead* self=raw;return xl6HttpIndex(index,self->value.FieldCount)?xl6HttpText(self->fields[index].Value):xl6TextFailure;}
xvalue* xl6Http1HeadGetBytesV2(void* raw,xstrview name)
{XL6_HttpHead* self=raw;const xhttpfield* field=xrtHttp1Field(&self->value,name);return field?xl6HttpBytes(xl6HttpAsBytes(field->Value)):NULL;}
xstrview xl6Http1HeadGetTextV2(void* raw,xstrview name)
{XL6_HttpHead* self=raw;const xhttpfield* field=xrtHttp1Field(&self->value,name);return field?xl6HttpText(field->Value):xl6TextFailure;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6Http1HeadHandlePolicyV2Get,xl6Http1HeadDestroyV2)
xvalue* xl6Http1HeadWireV2(void* raw)
{XL6_HttpHead* self=raw;return xl6HttpBytes((xbytesview){(const uint8*)(self->fields+self->value.FieldCount),self->value.Bytes});}
xvalue* xl6Http1HeadTargetBytesV2(void* raw){return xl6HttpBytes(xl6HttpAsBytes(((XL6_HttpHead*)raw)->value.Target));}
xvalue* xl6Http1HeadReasonBytesV2(void* raw){return xl6HttpBytes(xl6HttpAsBytes(((XL6_HttpHead*)raw)->value.Reason));}
