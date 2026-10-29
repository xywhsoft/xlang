#include "xl6_ws_stream_internal.h"
XL6_WsEvent* xl6WsEventCopyV2(int kind,const xwsmessageinfo* message,xbytesview data,size_t pending,const xwsstreamclose* close,const xerror* error)
{
    const char* em=error?xrtErrorMessage(error):NULL;const char* ed=error?xrtErrorDomain(error):NULL;
    size_t m=em?strlen(em):0,d=ed?strlen(ed):0,r=close?close->Reason.Size:0;
    size_t total=sizeof(XL6_WsEvent);
    const size_t sizes[]={data.Size,m,d,r};
    for(size_t i=0;i<4;++i){if(sizes[i]>SIZE_MAX-total){xrtSetErrorKind(XERR_RANGE);return NULL;}total+=sizes[i];}
    XL6_WsEvent* result=xrtCalloc(1,total);if(!result)return NULL;
    result->kind=kind;result->pending=pending;
    if(message)result->message=*message;
    if(close){result->close=*close;result->status=close->Transport;}
    result->errorCode=error?xrtErrorCode(error):0;
    char* at=result->storage;
    result->data=(xbytesview){(const uint8*)at,data.Size};if(data.Size)memcpy(at,data.Data,data.Size);at+=data.Size;
    result->errorMessage=(xstrview){at,m};if(m)memcpy(at,em,m);at+=m;
    result->errorDomain=(xstrview){at,d};if(d)memcpy(at,ed,d);at+=d;
    result->close.Reason=(xstrview){at,r};if(r)memcpy(at,close->Reason.Data,r);
    return result;
}
void xl6WsEventDestroyV2(void* p)
{if(p){XL6_WsEvent* event=p;xl6ExecutionResourceReleaseV1(event->stream);xrtFree(p);}}
#define WS_EVENT_INT(Name,Expression) int64_t xl6WsEvent##Name##V2(void* raw){const XL6_WsEvent* self=raw;return (Expression);}
WS_EVENT_INT(Kind,self->kind)
WS_EVENT_INT(Opcode,self->message.Opcode)
WS_EVENT_INT(Flags,self->message.Flags)
WS_EVENT_INT(Offset,(int64_t)self->message.Offset)
WS_EVENT_INT(Pending,(int64_t)self->pending)
WS_EVENT_INT(CloseFlags,self->close.Flags)
WS_EVENT_INT(LocalCode,self->close.LocalCode)
WS_EVENT_INT(RemoteCode,self->close.RemoteCode)
WS_EVENT_INT(Status,self->status)
WS_EVENT_INT(ErrorCode,self->errorCode)
#undef WS_EVENT_INT
uint64_t xl6WsEventPayloadSizeV2(void* raw){return ((XL6_WsEvent*)raw)->message.PayloadSize;}
uint64_t xl6WsEventDataSizeV2(void* raw)
{const XL6_WsEvent* e=raw;return e->kind==1?(e->data.Size?e->data.Size:e->pending):e->data.Size;}
void* xl6WsEventStreamV2(void* raw)
{
    XL6_ExecutionResourceV1* stream=((XL6_WsEvent*)raw)->stream;
    if(!stream){xrtSetErrorKind(XERR_STATE);return NULL;}
    return xl6ExecutionResourceRetainV1(stream);
}
xvalue* xl6WsEventDataV2(void* raw){return xrtValueBytes(((XL6_WsEvent*)raw)->data);}
xstrview xl6WsEventTextV2(void* raw){return xl6WsText(xl6WsAsText(((XL6_WsEvent*)raw)->data));}
xstrview xl6WsEventReasonV2(void* raw){return xl6TextCopy(((XL6_WsEvent*)raw)->close.Reason);}
xstrview xl6WsEventErrorMessageV2(void* raw){return xl6TextCopy(((XL6_WsEvent*)raw)->errorMessage);}
xstrview xl6WsEventErrorDomainV2(void* raw){return xl6TextCopy(((XL6_WsEvent*)raw)->errorDomain);}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=xl6WsEventHandlePolicyV2Get()->Ops)return false;
    const XL6_WsEvent* self=raw;
    return !self->stream||visit(xl6ExecutionResourceOwnershipV1(self->stream),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6WsEventHandlePolicyV2Get,xl6WsEventDestroyV2,trace)
