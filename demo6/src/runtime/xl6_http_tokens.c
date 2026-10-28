#include "xl6_http_internal.h"
XL6_HttpTokens* xl6HttpTokensCopyFields(const xhttpfield* fields,size_t count,xstrview name)
{
    size_t number=0,total=sizeof(XL6_HttpTokens);xhttpfieldtokencursor cursor;xstrview token;xhttpnext next;
    xrtHttpFieldTokenCursorInit(&cursor);
    while((next=xrtHttpFieldTokenNext(fields,count,name,&cursor,&token))==XHTTP_NEXT_ITEM){
        if(!xl6HttpAccumulate(&total,sizeof(xstrview))||!xl6HttpAccumulate(&total,token.Size))return NULL;
        ++number;
    }
    if(next==XHTTP_NEXT_ERROR)return NULL;
    XL6_HttpTokens* self=xrtMalloc(total);if(!self)return NULL;self->count=number;
    char* data=(char*)(self->items+number);xrtHttpFieldTokenCursorInit(&cursor);
    for(size_t i=0;i<number;++i){
        if(xrtHttpFieldTokenNext(fields,count,name,&cursor,&token)!=XHTTP_NEXT_ITEM){xrtFree(self);return NULL;}
        self->items[i]=xl6HttpStoreView(token,&data);
    }return self;
}
static void* fromValue(xstrview text)
{
    xstrview name={"List",4};xhttpfield field={name,text};return xl6HttpTokensCopyFields(&field,1,name);
}
void* xl6HttpTokensParseV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)?fromValue(text):NULL;}
void* xl6HttpTokensFromHeadersV2(void* raw,xstrview name)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return NULL;
 XL6_HttpTokens* result=xl6HttpTokensCopyFields(f,n,name);xl6HttpLeave(&g);return result;}
void xl6HttpTokensDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpTokensLengthV2(void* raw){return (int64_t)((XL6_HttpTokens*)raw)->count;}
xstrview xl6HttpTokensAtV2(void* raw,int64_t index)
{XL6_HttpTokens* self=raw;return xl6HttpIndex(index,self->count)?xl6TextCopy(self->items[index]):xl6TextFailure;}
bool xl6HttpTokensContainsV2(void* raw,xstrview token)
{XL6_HttpTokens* self=raw;for(size_t i=0;i<self->count;++i)if(xrtHttpTokenEqual(self->items[i],token))return true;return false;}
xstrview xl6HttpTokensToStringV2(void* raw)
{XL6_HttpTokens* self=raw;size_t size=0;char* data=xrtHttpTokenListBuild(self->items,self->count,&size);return xl6TextOwn(data,size);}
int64_t xl6HttpTokensWriteIntoV2(void* raw,void* target)
{
    XL6_HttpTokens* self=raw;xbuffer* output=xl6BufferChecked(target);size_t size=0;
    if(!output||!xrtHttpTokenListWrite(self->items,self->count,NULL,0,&size))return 0;
    if(size>(size_t)INT64_MAX-output->Size){xrtSetErrorKind(XERR_RANGE);return 0;}
    if(!xl6BufferAllocation(xrtBufferReserve(output,output->Size+size))||
       !xrtHttpTokenListWrite(self->items,self->count,output->Data?output->Data+output->Size:NULL,size,&size))return 0;
    output->Size+=size;return (int64_t)size;
}
typedef struct XL6_HttpWeightedTokens {size_t count;xhttpweightedtoken items[];} XL6_HttpWeightedTokens;
static void* weighted(xstrview text)
{
    size_t offset=0,count=0,total=sizeof(XL6_HttpWeightedTokens);xhttpweightedtoken item;xhttpnext next;
    while((next=xrtHttpWeightedTokenNext(text,&offset,&item))==XHTTP_NEXT_ITEM){
        if(!xl6HttpAccumulate(&total,sizeof(item))||!xl6HttpAccumulate(&total,item.Token.Size))return NULL;
        ++count;
    }
    if(next==XHTTP_NEXT_ERROR)return NULL;
    XL6_HttpWeightedTokens* self=xrtMalloc(total);if(!self)return NULL;self->count=count;
    char* data=(char*)(self->items+count);offset=0;
    for(size_t i=0;i<count;++i){if(xrtHttpWeightedTokenNext(text,&offset,&item)!=XHTTP_NEXT_ITEM){xrtFree(self);return NULL;}
        self->items[i]=item;self->items[i].Token=xl6HttpStoreView(item.Token,&data);}
    return self;
}
void* xl6HttpWeightedTokensParseV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)?weighted(text):NULL;}
void xl6HttpWeightedTokensDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpWeightedTokensLengthV2(void* raw){return (int64_t)((XL6_HttpWeightedTokens*)raw)->count;}
xstrview xl6HttpWeightedTokensAtV2(void* raw,int64_t index)
{XL6_HttpWeightedTokens* self=raw;return xl6HttpIndex(index,self->count)?xl6TextCopy(self->items[index].Token):xl6TextFailure;}
int64_t xl6HttpWeightedTokensQualityAtV2(void* raw,int64_t index)
{XL6_HttpWeightedTokens* self=raw;return xl6HttpIndex(index,self->count)?self->items[index].Quality:0;}
static void* list(xstrview text)
{
    size_t offset=0,count=0,total=sizeof(XL6_HttpTokens);xstrview item;xhttpnext next;
    while((next=xrtHttpListNext(text,offset,&offset,&item))==XHTTP_NEXT_ITEM){
        if(!xl6HttpAccumulate(&total,sizeof(item))||!xl6HttpAccumulate(&total,item.Size))return NULL;
        ++count;
    }
    if(next==XHTTP_NEXT_ERROR)return NULL;
    XL6_HttpTokens* self=xrtMalloc(total);if(!self)return NULL;self->count=count;
    char* data=(char*)(self->items+count);offset=0;
    for(size_t i=0;i<count;++i){if(xrtHttpListNext(text,offset,&offset,&item)!=XHTTP_NEXT_ITEM){xrtFree(self);return NULL;}
        self->items[i]=xl6HttpStoreView(item,&data);}
    return self;
}
void* xl6HttpListParseV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)?list(text):NULL;}
void xl6HttpListDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpListLengthV2(void* raw){return (int64_t)((XL6_HttpTokens*)raw)->count;}
xvalue* xl6HttpListAtV2(void* raw,int64_t index)
{XL6_HttpTokens* self=raw;return xl6HttpIndex(index,self->count)?xl6HttpBytes(xl6HttpAsBytes(self->items[index])):NULL;}
xstrview xl6HttpListTextAtV2(void* raw,int64_t index)
{XL6_HttpTokens* self=raw;return xl6HttpIndex(index,self->count)?xl6HttpText(self->items[index]):xl6TextFailure;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpTokensHandlePolicyV2Get,xl6HttpTokensDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpWeightedTokensHandlePolicyV2Get,xl6HttpWeightedTokensDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpListHandlePolicyV2Get,xl6HttpListDestroyV2)
XL6_HTTP_BYTE_PARSERS(xl6HttpTokensParse,fromValue(view))
XL6_HTTP_BYTE_PARSERS(xl6HttpWeightedTokensParse,weighted(view))
XL6_HTTP_BYTE_PARSERS(xl6HttpListParse,list(view))
