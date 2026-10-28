#include "xl6_http_body_internal.h"
typedef struct XL6_HttpBodyPlan {xhttp1bodyplan value;} XL6_HttpBodyPlan;
void* xl6HttpBodyPlanCreateV2(int64_t mode,uint64_t length)
{
    if(mode<0||mode>4||(mode!=XHTTP1_BODY_FIXED&&length)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_HttpBodyPlan* self=xrtMalloc(sizeof(*self));if(self)self->value=(xhttp1bodyplan){(xhttp1bodymode)mode,length};return self;
}
void xl6HttpBodyPlanDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpBodyPlanModeV2(void* raw){return ((XL6_HttpBodyPlan*)raw)->value.Mode;}
uint64_t xl6HttpBodyPlanLengthV2(void* raw){return ((XL6_HttpBodyPlan*)raw)->value.Length;}
static bool headPlan(void* raw,xstrview method,xhttp1bodyplan* plan)
{
    XL6_HttpHead* head=raw;
    return head->value.Kind==XHTTP_REQUEST?xrtHttp1RequestBodyPlan(&head->value,plan):xrtHttp1ResponseBodyPlan(&head->value,method,plan);
}
void* xl6Http1HeadBodyPlanV2(void* raw,xstrview method)
{
    xhttp1bodyplan plan;if(!headPlan(raw,method,&plan))return NULL;
    return xl6HttpBodyPlanCreateV2(plan.Mode,plan.Length);
}
int64_t xl6Http1HeadBodyModeV2(void* raw,xstrview method)
{xhttp1bodyplan plan;return headPlan(raw,method,&plan)?(int64_t)plan.Mode:0;}
static bool prepare(const xhttp1bodyplan* plan,int64_t maxBody,int64_t chunk,int64_t trailer,int64_t line,int64_t fields,
    xhttp1body* body,xhttpfield** scratch)
{
    xhttp1bodylimits limits;
    if(!xl6HttpBodyLimits(&limits,maxBody,chunk,trailer,line,fields))return false;
    size_t capacity=plan->Mode==XHTTP1_BODY_CHUNKED?limits.MaxTrailers:0;
    if(capacity>SIZE_MAX/sizeof(xhttpfield)){xrtSetErrorKind(XERR_RANGE);return false;}
    *scratch=capacity?xrtMalloc(capacity*sizeof(xhttpfield)):NULL;if(capacity&&!*scratch)return false;
    if(!xrtHttp1BodyInit(body,plan,*scratch,capacity,&limits)){xrtFree(*scratch);*scratch=NULL;return false;}return true;
}
static void* create(const xhttp1bodyplan* plan,int64_t maxBody,int64_t chunk,int64_t trailer,int64_t line,int64_t fields)
{
    xhttp1body body;xhttpfield* scratch=NULL;if(!prepare(plan,maxBody,chunk,trailer,line,fields,&body,&scratch))return NULL;
    XL6_HttpBodyReader* self=xrtCalloc(1,sizeof(*self));if(!self){xrtFree(scratch);return NULL;}self->body=body;self->scratch=scratch;return self;
}
void* xl6Http1BodyReaderCreateV2(void* raw,int64_t maxBody,int64_t chunk,int64_t trailer,int64_t line,int64_t fields)
{return create(&((XL6_HttpBodyPlan*)raw)->value,maxBody,chunk,trailer,line,fields);}
void* xl6Http1BodyReaderCreateHeadV2(void* raw,xstrview method,int64_t maxBody,int64_t chunk,int64_t trailer,int64_t line,int64_t fields)
{
    xhttp1bodyplan plan;bool ok=headPlan(raw,method,&plan);
    return ok?create(&plan,maxBody,chunk,trailer,line,fields):NULL;
}
void xl6Http1BodyReaderDestroyV2(void* raw)
{XL6_HttpBodyReader* self=raw;if(self){xl6HttpHeadersDispose(self->trailers);xrtFree(self->scratch);xrtFree(self);}}
void xl6Http1BodyReaderResetV2(void* raw,void* plan,int64_t maxBody,int64_t chunk,int64_t trailer,int64_t line,int64_t fields)
{
    XL6_HttpBodyReader* self=raw;XL6_HttpGuard g={0};if(!xl6HttpEnter(&self->gate,&g))return;
    xhttp1body next;xhttpfield* scratch=NULL;
    if(prepare(&((XL6_HttpBodyPlan*)plan)->value,maxBody,chunk,trailer,line,fields,&next,&scratch)){
        xrtFree(self->scratch);xl6HttpHeadersDispose(self->trailers);self->trailers=NULL;
        self->scratch=scratch;self->body=next;self->failed=false;self->consumed=0;
    }xl6HttpLeave(&g);
}
int64_t xl6HttpBodyReadLocked(XL6_HttpBodyReader* self,xbytesview input,xbuffer* out,bool end)
{
    if(self->failed){xrtSetErrorKind(XERR_STATE);return 0;}
    xhttp1body next=self->body;xhttp1errorinfo error={0};xbytesview data={0};size_t consumed=0;
    xhttp1bodystatus state=xrtHttp1BodyRead(&next,input,end,&consumed,&data,&error);bool ok=true;
    XL6_HttpHeaders* trailers=NULL;
    if(state==XHTTP1_BODY_ERROR){xl6HttpParseError(&error);ok=false;}
    else if(state==XHTTP1_BODY_FIELDS){xrtSetErrorKind(XERR_INTERNAL);ok=false;}
    else if(state==XHTTP1_BODY_DATA&&out)ok=xl6HttpAppend(out,data);
    else if(state==XHTTP1_BODY_DONE&&next.TrailerCount&&!self->trailers){
        trailers=xl6HttpHeadersCopyFields(next.Trailers,next.TrailerCount);ok=trailers!=NULL;
    }
    if(ok){self->body=next;self->consumed=consumed;if(trailers)self->trailers=trailers;}
    else self->failed=true;
    return ok?(int64_t)state:0;
}
int64_t xl6Http1BodyReaderReadSpanV2(void* raw,xbytesview input,void* target,bool end,int64_t size)
{
    XL6_HttpBodyReader* self=raw;xbuffer* out;XL6_HttpGuard g={0};
    if(!xl6BufferSelect(&input,size)||!xl6HttpOutput(target,input,&out)||!xl6HttpEnter(&self->gate,&g))return 0;
    int64_t state=xl6HttpBodyReadLocked(self,input,out,end);xl6HttpLeave(&g);return state;
}
int64_t xl6Http1BodyReaderReadBytesV2(void* raw,xvalue* input,void* out,bool end,int64_t size)
{xbytesview view;return xl6BufferBytesInput(input,size,&view)?xl6Http1BodyReaderReadSpanV2(raw,view,out,end,-1):0;}
int64_t xl6Http1BodyReaderReadBufferV2(void* raw,void* input,void* out,bool end,int64_t size)
{xbytesview view;return xl6BufferInput(input,size,&view)?xl6Http1BodyReaderReadSpanV2(raw,view,out,end,-1):0;}
#define QUERY(Name,Type,Value,Failure) Type xl6Http1BodyReader##Name##V2(void* raw) \
{XL6_HttpBodyReader* self=raw;XL6_HttpGuard g={0};if(!xl6HttpEnter(&self->gate,&g))return Failure;Type value=(Value);xl6HttpLeave(&g);return value;}
QUERY(Done,bool,xrtHttp1BodyDone(&self->body),false)
QUERY(Failed,bool,self->failed,false)
QUERY(Consumed,int64_t,(int64_t)self->consumed,0)
QUERY(Received,uint64_t,self->body.Received,0)
QUERY(WireBytes,uint64_t,self->body.WireBytes,0)
QUERY(Remaining,uint64_t,self->body.Remaining,0)
QUERY(Mode,int64_t,(int64_t)self->body.Mode,0)
#undef QUERY
void* xl6Http1BodyReaderTrailersV2(void* raw)
{
    XL6_HttpBodyReader* self=raw;XL6_HttpGuard g={0};if(!xl6HttpEnter(&self->gate,&g))return NULL;
    void* result=NULL;
    if(!xrtHttp1BodyDone(&self->body)||self->failed)xrtSetErrorKind(XERR_STATE);
    else if(self->trailers)result=xl6HttpHeadersCloneV2(self->trailers);
    else result=xl6HttpHeadersCreateV2(0);
    xl6HttpLeave(&g);return result;
}
void* xl6Http1ParseTrailersV2(void* raw,int64_t maxBody,int64_t chunk,int64_t trailer,int64_t line,int64_t count,int64_t size)
{
    xbytesview input;xhttp1bodylimits limits;
    if(!xl6BufferInput(raw,size,&input)||!xl6HttpBodyLimits(&limits,maxBody,chunk,trailer,line,count))return NULL;
    if(limits.MaxTrailers>SIZE_MAX/sizeof(xhttpfield)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    xhttpfield* fields=limits.MaxTrailers?xrtMalloc(limits.MaxTrailers*sizeof(*fields)):NULL;
    if(limits.MaxTrailers&&!fields)return NULL;
    size_t used=0,number=0;xhttp1errorinfo error={0};
    xhttp1status status=xrtHttp1TrailersParse(input,fields,limits.MaxTrailers,&limits,&used,&number,&error);
    XL6_HttpHeaders* result=status==XHTTP1_READY?xl6HttpHeadersCopyFields(fields,number):NULL;
    if(result)result->parsedBytes=(int64_t)used;
    if(status==XHTTP1_ERROR)xl6HttpParseError(&error);
    else if(status==XHTTP1_FIELDS)xrtSetErrorKind(XERR_INTERNAL);
    xrtFree(fields);return result;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpBodyPlanHandlePolicyV2Get,xl6HttpBodyPlanDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6Http1BodyReaderHandlePolicyV2Get,xl6Http1BodyReaderDestroyV2)
