#include "xl6_pem_internal.h"
#include "xl6_text_thread.h"
typedef struct Reader {XL6_TextThreadOwner owner;str text;xpemcursor cursor;size_t limit;} Reader;
static Reader* check(void* raw)
{Reader* self=raw;if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return xl6TextThreadCheck(&self->owner)?self:NULL;}
void xl6PemReaderDestroyV1(void* raw)
{
    Reader* self=raw;if (!self) return;xerror* error=xrtTakeError();xl6TextThreadDrop(&self->owner);
    xl6CryptoFreeSecret(self->text,self->cursor.Text.Size);xl6CryptoFreeSecret(self,sizeof(*self));xrtClearError();xrtSetErrorTake(error);
}
void* xl6PemReaderCreateV1(xstrview text,int64_t size,int64_t limit)
{
    if (!xl6TextSelectRaw(&text,size) || !xl6DerBound((xbytesview){(const uint8*)text.Data,text.Size},limit)) return NULL;
    Reader* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    self->limit=(size_t)limit;
    if (!xl6TextThreadInit(&self->owner) || !(self->text=xrtStrDupView(text)) ||
        !xrtPemInit(&self->cursor,self->text,text.Size)) {xl6PemReaderDestroyV1(self);return NULL;}
    return self;
}
bool xl6PemReaderDoneV1(void* raw)
{
    Reader* self=check(raw);if (!self) return false;
    xpemcursor next=self->cursor;xpemblock block;
    return xrtPemRead(&next,&block)==XPEM_DONE;
}
void* xl6PemReaderReadV1(void* raw)
{
    Reader* self=check(raw);if (!self) return NULL;
    xpemcursor next=self->cursor;xpemblock block;xpemresult status=xrtPemRead(&next,&block);
    if (status!=XPEM_BLOCK) {if (status==XPEM_DONE) xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_PemBlock* result=xl6PemCopy(&block,self->limit);
    if (result) self->cursor=next;
    return result;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6PemReaderHandlePolicyV1Get,xl6PemReaderDestroyV1)
