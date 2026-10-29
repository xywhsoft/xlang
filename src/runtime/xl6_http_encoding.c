#include "xl6_http_internal.h"
typedef struct XL6_HttpAcceptEncoding {xatomic32 gate;xhttpacceptencoding value;} XL6_HttpAcceptEncoding;
typedef struct XL6_HttpContentEncodings {
    xhttpcontentencodingplan plan;xstrview joined;xhttpcontentencodingitem items[];
} XL6_HttpContentEncodings;
void* xl6HttpAcceptEncodingCreateV2(void)
{XL6_HttpAcceptEncoding* self=xrtCalloc(1,sizeof(*self));if(self)xrtHttpAcceptEncodingInit(&self->value);return self;}
void xl6HttpAcceptEncodingDestroyV2(void* raw){xrtFree(raw);}
void xl6HttpAcceptEncodingResetV2(void* raw)
{XL6_HttpAcceptEncoding* self=raw;XL6_HttpGuard g={0};if(!xl6HttpEnter(&self->gate,&g))return;
 xrtHttpAcceptEncodingInit(&self->value);xl6HttpLeave(&g);}
void* xl6HttpAcceptEncodingFromHeadersV2(void* raw)
{
    XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return NULL;
    XL6_HttpAcceptEncoding* self=xl6HttpAcceptEncodingCreateV2();bool ok=self&&xrtHttpAcceptEncodingParse(f,n,&self->value);
    xl6HttpLeave(&g);if(!ok){xrtFree(self);self=NULL;}return self;
}
void xl6HttpAcceptEncodingAddV2(void* raw,xstrview text,int64_t size)
{XL6_HttpAcceptEncoding* self=raw;XL6_HttpGuard g={0};if(!xl6TextSelectRaw(&text,size)||!xl6HttpEnter(&self->gate,&g))return;
 xrtHttpAcceptEncodingAdd(&self->value,text);xl6HttpLeave(&g);}
int64_t xl6HttpAcceptEncodingFlagsV2(void* raw)
{XL6_HttpAcceptEncoding* self=raw;XL6_HttpGuard g={0};if(!xl6HttpEnter(&self->gate,&g))return 0;int64_t flags=self->value.Flags;xl6HttpLeave(&g);return flags;}
bool xl6HttpAcceptEncodingValidV2(void* raw)
{XL6_HttpAcceptEncoding* self=raw;XL6_HttpGuard g={0};if(!xl6HttpEnter(&self->gate,&g))return false;bool valid=xrtHttpAcceptEncodingValid(&self->value);xl6HttpLeave(&g);return valid;}
int64_t xl6HttpAcceptEncodingQualityV2(void* raw,int64_t coding)
{XL6_HttpAcceptEncoding* self=raw;XL6_HttpGuard g={0};if(coding<0||coding>7){xrtSetErrorKind(XERR_RANGE);return 0;}
 if(!xl6HttpEnter(&self->gate,&g))return 0;
 uint16 value=xrtHttpAcceptEncodingQuality(&self->value,(xhttpcoding)coding);xl6HttpLeave(&g);return value;}
int64_t xl6HttpAcceptEncodingSelectV2(void* raw,int64_t available,int64_t preferred)
{XL6_HttpAcceptEncoding* self=raw;XL6_HttpGuard g={0};if(available<0||available>7||preferred<0||preferred>7){xrtSetErrorKind(XERR_RANGE);return 0;}
 if(!xl6HttpEnter(&self->gate,&g))return 0;
 xhttpcoding value=xrtHttpAcceptEncodingSelect(&self->value,(uint32)available,(xhttpcoding)preferred);xl6HttpLeave(&g);return value;}
void* xl6HttpContentEncodingsFromHeadersV2(void* raw)
{
    XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return NULL;
    xhttpcontentencodingplan plan;size_t joined=0,total=sizeof(XL6_HttpContentEncodings);
    bool ok=xrtHttpContentEncodingPlan(f,n,&plan);
    if(ok)joined=plan.JoinedSize;
    if(ok&&(plan.CodingCount>SIZE_MAX/sizeof(xhttpcontentencodingitem)||
       !xl6HttpAccumulate(&total,plan.CodingCount*sizeof(xhttpcontentencodingitem))||!xl6HttpAccumulate(&total,joined)))ok=false;
    xhttpcontentencodingcursor cursor;xhttpcontentencodingitem item;xhttpnext next=XHTTP_NEXT_END;
    xrtHttpContentEncodingCursorInit(&cursor);
    while(ok&&(next=xrtHttpContentEncodingNext(f,n,&cursor,&item))==XHTTP_NEXT_ITEM)ok=xl6HttpAccumulate(&total,item.Token.Size);
    if(next==XHTTP_NEXT_ERROR)ok=false;
    XL6_HttpContentEncodings* self=ok?xrtMalloc(total):NULL;
    if(self){self->plan=plan;char* data=(char*)(self->items+plan.CodingCount);
        self->joined=(xstrview){data,joined};ok=xrtHttpContentEncodingWrite(f,n,data,joined,&joined);data+=joined;
        xrtHttpContentEncodingCursorInit(&cursor);
        for(size_t i=0;ok&&i<plan.CodingCount;++i){ok=xrtHttpContentEncodingNext(f,n,&cursor,&item)==XHTTP_NEXT_ITEM;
            if(ok){self->items[i]=item;self->items[i].Token=xl6HttpStoreView(item.Token,&data);}}
        if(!ok){xrtFree(self);self=NULL;}
    }
    xl6HttpLeave(&g);return self;
}
void xl6HttpContentEncodingsDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpContentEncodingsLengthV2(void* raw){return (int64_t)((XL6_HttpContentEncodings*)raw)->plan.CodingCount;}
int64_t xl6HttpContentEncodingsFieldCountV2(void* raw){return (int64_t)((XL6_HttpContentEncodings*)raw)->plan.FieldCount;}
int64_t xl6HttpContentEncodingsDecoderCountV2(void* raw){return (int64_t)((XL6_HttpContentEncodings*)raw)->plan.DecoderCount;}
int64_t xl6HttpContentEncodingsUnknownCountV2(void* raw){return (int64_t)((XL6_HttpContentEncodings*)raw)->plan.UnknownCount;}
int64_t xl6HttpContentEncodingsFlagsV2(void* raw){return ((XL6_HttpContentEncodings*)raw)->plan.Flags;}
xstrview xl6HttpContentEncodingsNameAtV2(void* raw,int64_t index)
{XL6_HttpContentEncodings* self=raw;return xl6HttpIndex(index,self->plan.CodingCount)?xl6TextCopy(self->items[index].Token):xl6TextFailure;}
int64_t xl6HttpContentEncodingsCodingAtV2(void* raw,int64_t index)
{XL6_HttpContentEncodings* self=raw;return xl6HttpIndex(index,self->plan.CodingCount)?self->items[index].Coding:0;}
xstrview xl6HttpContentEncodingsToStringV2(void* raw){return xl6TextCopy(((XL6_HttpContentEncodings*)raw)->joined);}
int64_t xl6HttpContentEncodingsWriteIntoV2(void* raw,void* target)
{xbuffer* out=xl6BufferChecked(target);xstrview text=((XL6_HttpContentEncodings*)raw)->joined;return out&&xl6HttpAppend(out,xl6HttpAsBytes(text))?(int64_t)text.Size:0;}
int64_t xl6HttpCodingV2(xstrview token){return xrtHttpCodingParse(token);}
xstrview xl6HttpCodingNameV2(int64_t coding)
{if(coding<0||coding>7){xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;}return xl6TextCopy(xrtHttpCodingName((xhttpcoding)coding));}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpAcceptEncodingHandlePolicyV2Get,xl6HttpAcceptEncodingDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpContentEncodingsHandlePolicyV2Get,xl6HttpContentEncodingsDestroyV2)
int64_t xl6HttpHeadersSelectEncodingV2(void* raw,int64_t available,int64_t preferred)
{
    if(available<0||available>7||preferred<0||preferred>7){xrtSetErrorKind(XERR_RANGE);return 0;}
    XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return 0;
    xhttpacceptencoding value;xhttpcoding selected=XHTTP_CODING_NONE;
    if(xrtHttpAcceptEncodingParse(f,n,&value))selected=xrtHttpAcceptEncodingSelect(&value,(uint32)available,(xhttpcoding)preferred);
    xl6HttpLeave(&g);return selected;
}
