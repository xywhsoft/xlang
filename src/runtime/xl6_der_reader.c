#include "xl6_der_internal.h"
#include "xl6_text_thread.h"
typedef struct Reader {XL6_TextThreadOwner owner;bytes data;xdercursor cursor;} Reader;
static Reader* check(void* raw)
{Reader* self=raw;if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return xl6TextThreadCheck(&self->owner)?self:NULL;}
void xl6DerReaderDestroyV1(void* raw)
{
    Reader* self=raw;if (!self) return;xerror* error=xrtTakeError();
    xl6TextThreadDrop(&self->owner);xl6CryptoFreeSecret(self->data,self->cursor.Size);xl6CryptoFreeSecret(self,sizeof(*self));
    xrtClearError();xrtSetErrorTake(error);
}
static Reader* create(xbytesview data)
{
    Reader* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    if (!xl6TextThreadInit(&self->owner) || !(self->data=xrtMalloc(data.Size?data.Size:1))) {xl6DerReaderDestroyV1(self);return NULL;}
    if (data.Size) memcpy(self->data,data.Data,data.Size);
    if (!xrtDerInit(&self->cursor,self->data,data.Size)) {xl6DerReaderDestroyV1(self);return NULL;}
    return self;
}
void* xl6DerReaderCreateV1(const xvalue* value,int64_t size,int64_t limit)
{xbytesview data;return xl6BufferBytesInput(value,size,&data) && xl6DerBound(data,limit)?create(data):NULL;}
void* xl6DerValueReaderV1(void* raw)
{
    XL6_DerValue* self=xl6DerChecked(raw);xdercursor child;
    return self && xrtDerEnter(&self->value,&child)?create(self->value.Value):NULL;
}
bool xl6DerReaderDoneV1(void* raw)
{Reader* self=check(raw);return self && xrtDerDone(&self->cursor);}
int64_t xl6DerReaderRemainingV1(void* raw)
{Reader* self=check(raw);return self?(int64_t)xrtDerRemaining(&self->cursor):0;}
static void* read(void* raw,bool peek)
{
    Reader* self=check(raw);if (!self) return NULL;
    xdercursor next=self->cursor;xdervalue value;
    xderresult status=xrtDerRead(&next,&value);
    if (status!=XDER_VALUE) {if (status==XDER_DONE) xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_DerValue* result=xl6DerCopyValue(value.Raw);
    /* Both malformed input and allocation failure preserve the old position. */
    if (result && !peek) self->cursor=next;
    return result;
}
void* xl6DerReaderReadV1(void* raw) {return read(raw,false);}
void* xl6DerReaderPeekV1(void* raw) {return read(raw,true);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6DerReaderHandlePolicyV1Get,xl6DerReaderDestroyV1)
