#include "xl6_http_internal.h"
typedef struct XL6_HttpTe {xhttpteinfo info;size_t count;xhttpfield* fields;xhttptecoding items[];} XL6_HttpTe;
static void* make(const xhttpfield* fields,size_t count)
{
    xhttpteinfo info;if(!xrtHttpTeParse(fields,count,&info))return NULL;
    size_t total=sizeof(XL6_HttpTe),number=0;xhttptefieldcursor cursor;xhttptecoding item;xhttpnext next;
    xrtHttpTeFieldCursorInit(&cursor);
    while((next=xrtHttpTeFieldNext(fields,count,&cursor,&item))==XHTTP_NEXT_ITEM){
        if(!xl6HttpAccumulate(&total,sizeof(item)+sizeof(xhttpfield))||!xl6HttpAccumulate(&total,item.Element.Size))return NULL;
        ++number;
    }
    if(next==XHTTP_NEXT_ERROR)return NULL;
    XL6_HttpTe* self=xrtMalloc(total);if(!self)return NULL;
    self->info=info;self->count=number;self->fields=(xhttpfield*)(self->items+number);
    char* data=(char*)(self->fields+number);xrtHttpTeFieldCursorInit(&cursor);
    for(size_t i=0;i<number;++i){if(xrtHttpTeFieldNext(fields,count,&cursor,&item)!=XHTTP_NEXT_ITEM){xrtFree(self);return NULL;}
        xstrview copy=xl6HttpStoreView(item.Element,&data);self->items[i]=item;self->items[i].Element=copy;
        self->items[i].Coding=xl6HttpRebase(item.Coding,item.Element.Data,copy.Data);
        self->items[i].Parameters=xl6HttpRebase(item.Parameters,item.Element.Data,copy.Data);
        self->fields[i]=(xhttpfield){{"TE",2},copy};
    }return self;
}
void* xl6HttpTeParseV2(xstrview text,int64_t size)
{if(!xl6TextSelectRaw(&text,size))return NULL;xhttpfield field={{"TE",2},text};return make(&field,1);}
void* xl6HttpTeFromHeadersV2(void* raw)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return NULL;void* result=make(f,n);xl6HttpLeave(&g);return result;}
void xl6HttpTeDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpTeLengthV2(void* raw){return (int64_t)((XL6_HttpTe*)raw)->count;}
int64_t xl6HttpTeFieldCountV2(void* raw){return (int64_t)((XL6_HttpTe*)raw)->info.FieldCount;}
int64_t xl6HttpTeTransferCodingCountV2(void* raw){return (int64_t)((XL6_HttpTe*)raw)->info.TransferCodingCount;}
int64_t xl6HttpTeFlagsV2(void* raw){return ((XL6_HttpTe*)raw)->info.Flags;}
bool xl6HttpTeAcceptsTrailersV2(void* raw){return (((XL6_HttpTe*)raw)->info.Flags&XHTTP_TE_ACCEPTS_TRAILERS)!=0;}
static const xhttptecoding* at(void* raw,int64_t index)
{XL6_HttpTe* self=raw;return xl6HttpIndex(index,self->count)?self->items+index:NULL;}
xstrview xl6HttpTeCodingAtV2(void* raw,int64_t index)
{const xhttptecoding* item=at(raw,index);return item?xl6TextCopy(item->Coding):xl6TextFailure;}
int64_t xl6HttpTeQualityAtV2(void* raw,int64_t index)
{const xhttptecoding* item=at(raw,index);return item?item->Quality:0;}
int64_t xl6HttpTeFlagsAtV2(void* raw,int64_t index)
{const xhttptecoding* item=at(raw,index);return item?item->Flags:0;}
void* xl6HttpTeParametersAtV2(void* raw,int64_t index)
{const xhttptecoding* item=at(raw,index);return item?xl6HttpParametersParseV2(item->Parameters,-1):NULL;}
int64_t xl6HttpTeQualityV2(void* raw,xstrview coding)
{XL6_HttpTe* self=raw;return xrtHttpTeQuality(self->fields,self->count,coding);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpTeHandlePolicyV2Get,xl6HttpTeDestroyV2)
XL6_HTTP_BYTE_PARSERS(xl6HttpTeParse,make(&(xhttpfield){{"TE",2},view},1))
int64_t xl6HttpHeadersTeQualityV2(void* raw,xstrview coding)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return 0;
 uint16 result=xrtHttpTeQuality(f,n,coding);xl6HttpLeave(&g);return result;}
bool xl6HttpHeadersTeAcceptsTrailersV2(void* raw)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return false;
 xhttpnext result=xrtHttpTeAcceptsTrailers(f,n);xl6HttpLeave(&g);return result==XHTTP_NEXT_ITEM;}
