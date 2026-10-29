#include "xl6_http_internal.h"
typedef struct XL6_HttpTransferCodings {size_t count;xhttp1transfercoding items[];} XL6_HttpTransferCodings;
static void* parse(xstrview text)
{
    size_t count=0,total=sizeof(XL6_HttpTransferCodings),offset=0;xhttp1transfercoding item;xhttpnext next;
    while((next=xrtHttp1TransferCodingNext(text,&offset,&item))==XHTTP_NEXT_ITEM){
        if(!xl6HttpAccumulate(&total,sizeof(item))||!xl6HttpAccumulate(&total,item.Name.Size)||!xl6HttpAccumulate(&total,item.Parameters.Size))return NULL;
        ++count;
    }
    if(next==XHTTP_NEXT_ERROR)return NULL;
    XL6_HttpTransferCodings* self=xrtMalloc(total);if(!self)return NULL;self->count=count;
    char* data=(char*)(self->items+count);offset=0;
    for(size_t i=0;i<count;++i){if(xrtHttp1TransferCodingNext(text,&offset,&item)!=XHTTP_NEXT_ITEM){xrtFree(self);return NULL;}
        self->items[i]=(xhttp1transfercoding){xl6HttpStoreView(item.Name,&data),xl6HttpStoreView(item.Parameters,&data)};
    }return self;
}
void* xl6HttpTransferCodingsParseV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)?parse(text):NULL;}
void xl6HttpTransferCodingsDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpTransferCodingsLengthV2(void* raw){return (int64_t)((XL6_HttpTransferCodings*)raw)->count;}
xstrview xl6HttpTransferCodingsNameAtV2(void* raw,int64_t index)
{XL6_HttpTransferCodings* self=raw;return xl6HttpIndex(index,self->count)?xl6TextCopy(self->items[index].Name):xl6TextFailure;}
void* xl6HttpTransferCodingsParametersAtV2(void* raw,int64_t index)
{XL6_HttpTransferCodings* self=raw;return xl6HttpIndex(index,self->count)?xl6HttpParametersFragment(self->items[index].Parameters):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpTransferCodingsHandlePolicyV2Get,xl6HttpTransferCodingsDestroyV2)
XL6_HTTP_BYTE_PARSERS(xl6HttpTransferCodingsParse,parse(view))
