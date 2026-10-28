#include "xl6_http_internal.h"
typedef struct XL6_HttpUpgrades {size_t count;xhttpupgradeitem items[];} XL6_HttpUpgrades;
static void* make(const xhttpfield* fields,size_t count)
{
    size_t number=0,total=sizeof(XL6_HttpUpgrades);xhttpupgradefieldcursor cursor;xhttpupgradeitem item;xhttpnext next;
    xrtHttpUpgradeFieldCursorInit(&cursor);
    while((next=xrtHttpUpgradeFieldNext(fields,count,&cursor,&item))==XHTTP_NEXT_ITEM){
        if(!xl6HttpAccumulate(&total,sizeof(item))||!xl6HttpAccumulate(&total,item.Protocol.Size)||!xl6HttpAccumulate(&total,item.Version.Size))return NULL;
        ++number;
    }
    if(next==XHTTP_NEXT_ERROR)return NULL;
    XL6_HttpUpgrades* self=xrtMalloc(total);if(!self)return NULL;self->count=number;
    char* data=(char*)(self->items+number);xrtHttpUpgradeFieldCursorInit(&cursor);
    for(size_t i=0;i<number;++i){if(xrtHttpUpgradeFieldNext(fields,count,&cursor,&item)!=XHTTP_NEXT_ITEM){xrtFree(self);return NULL;}
        self->items[i]=(xhttpupgradeitem){xl6HttpStoreView(item.Protocol,&data),xl6HttpStoreView(item.Version,&data)};
    }return self;
}
void* xl6HttpUpgradesParseV2(xstrview text,int64_t size)
{if(!xl6TextSelectRaw(&text,size))return NULL;xhttpfield field={{"Upgrade",7},text};return make(&field,1);}
void* xl6HttpUpgradesFromHeadersV2(void* raw)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return NULL;void* result=make(f,n);xl6HttpLeave(&g);return result;}
void xl6HttpUpgradesDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpUpgradesLengthV2(void* raw){return (int64_t)((XL6_HttpUpgrades*)raw)->count;}
xstrview xl6HttpUpgradesProtocolAtV2(void* raw,int64_t index)
{XL6_HttpUpgrades* self=raw;return xl6HttpIndex(index,self->count)?xl6TextCopy(self->items[index].Protocol):xl6TextFailure;}
xstrview xl6HttpUpgradesVersionAtV2(void* raw,int64_t index)
{XL6_HttpUpgrades* self=raw;return xl6HttpIndex(index,self->count)?xl6TextCopy(self->items[index].Version):xl6TextFailure;}
xstrview xl6HttpUpgradesToStringV2(void* raw)
{XL6_HttpUpgrades* self=raw;size_t size=0;char* data=xrtHttpUpgradeBuild(self->items,self->count,&size);return xl6TextOwn(data,size);}
int64_t xl6HttpUpgradesWriteIntoV2(void* raw,void* target)
{
    XL6_HttpUpgrades* self=raw;xbuffer* out=xl6BufferChecked(target);size_t size=0;
    if(!out||!xrtHttpUpgradeWrite(self->items,self->count,NULL,0,&size))return 0;
    if(size>(size_t)INT64_MAX-out->Size){xrtSetErrorKind(XERR_RANGE);return 0;}
    if(!xl6BufferAllocation(xrtBufferReserve(out,out->Size+size))||!xrtHttpUpgradeWrite(self->items,self->count,out->Data?out->Data+out->Size:NULL,size,&size))return 0;
    out->Size+=size;return (int64_t)size;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpUpgradesHandlePolicyV2Get,xl6HttpUpgradesDestroyV2)
XL6_HTTP_BYTE_PARSERS(xl6HttpUpgradesParse,make(&(xhttpfield){{"Upgrade",7},view},1))
int64_t xl6HttpUpgradesWriteElementIntoV2(void* raw,int64_t index,void* target)
{
    XL6_HttpUpgrades* self=raw;xbuffer* out=xl6BufferChecked(target);size_t size=0;
    if(!out||!xl6HttpIndex(index,self->count)||!xrtHttpUpgradeElementWrite(self->items+index,NULL,0,&size))return 0;
    if(size>(size_t)INT64_MAX-out->Size){xrtSetErrorKind(XERR_RANGE);return 0;}
    if(!xl6BufferAllocation(xrtBufferReserve(out,out->Size+size))||!xrtHttpUpgradeElementWrite(self->items+index,out->Data?out->Data+out->Size:NULL,size,&size))return 0;
    out->Size+=size;return (int64_t)size;
}
xstrview xl6HttpUpgradesElementAtV2(void* raw,int64_t index)
{
    xbuffer output={0};xl6HttpUpgradesWriteElementIntoV2(raw,index,&output);
    xstrview result=xrtGetError()?xl6TextFailure:xl6TextCopy((xstrview){(const char*)output.Data,output.Size});
    xrtBufferUnit(&output);return result;
}
