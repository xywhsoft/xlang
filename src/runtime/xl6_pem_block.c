#include "xl6_pem_internal.h"
static XL6_PemBlock* check(void* raw)
{if (!raw) xrtSetErrorKind(XERR_ARGUMENT);return raw;}
void xl6PemBlockDestroyV1(void* raw)
{XL6_PemBlock* self=raw;if (!self) return;xl6CryptoFreeSecret(self->text,self->block.Raw.Size);xl6CryptoFreeSecret(self,sizeof(*self));}
xstrview xl6PemBlockLabelV1(void* raw)
{XL6_PemBlock* self=check(raw);return self?xl6TextCopy(self->block.Label):xl6TextFailure;}
xstrview xl6PemBlockRawV1(void* raw)
{XL6_PemBlock* self=check(raw);return self?xl6TextCopy(self->block.Raw):xl6TextFailure;}
xvalue* xl6PemBlockDecodeV1(void* raw)
{
    XL6_PemBlock* self=check(raw);size_t size;if (!self) return NULL;
    if (!xrtPemDecode(&self->block,NULL,0,&size)) return NULL;
    if (size>self->limit) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    bytes data=xrtMalloc(size?size:1);if (!data) return NULL;
    bool ok=xrtPemDecode(&self->block,data,size,&size);
    return xl6CryptoTakeSecret(data,size,ok);
}
void* xl6PemFindV1(xstrview text,xstrview label,int64_t size,int64_t limit)
{
    if (!xl6TextSelectRaw(&text,size) || !xl6DerBound((xbytesview){(const uint8*)text.Data,text.Size},limit)) return NULL;
    xpemblock block;
    if (!label.Size) {
        xpemcursor cursor;
        if (!xrtPemInit(&cursor,text.Data,text.Size)) return NULL;
        xpemresult result=xrtPemRead(&cursor,&block);
        if (result!=XPEM_BLOCK) {if (result==XPEM_DONE) xrtSetErrorKind(XERR_RANGE);return NULL;}
    } else {
        str name;if (!xl6PemLabel(label,&name)) return NULL;
        bool ok=xrtPemFind(text.Data,text.Size,name,&block);xl6CryptoFreeSecret(name,label.Size);
        if (!ok) return NULL;
    }
    return xl6PemCopy(&block,(size_t)limit);
}
xstrview xl6PemEncodeV1(xstrview label,const xvalue* value,int64_t size)
{
    xbytesview data;str name;
    if (!xl6BufferBytesInput(value,size,&data) || !xl6PemLabel(label,&name)) return xl6TextFailure;
    size_t length=0;bool ok=xrtPemEncode(name,data.Data,data.Size,NULL,0,&length);
    if (!ok || length>INT64_MAX || length==SIZE_MAX) {
        if (ok) xrtSetErrorKind(XERR_RANGE);
        xl6CryptoFreeSecret(name,label.Size);return xl6TextFailure;
    }
    str result=xrtMalloc(length+1);
    if (result && !xrtPemEncode(name,data.Data,data.Size,result,length+1,&length)) {xl6CryptoFreeSecret(result,length);result=NULL;}
    xl6CryptoFreeSecret(name,label.Size);return xl6TextOwn(result,length);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6PemBlockHandlePolicyV1Get,xl6PemBlockDestroyV1)
