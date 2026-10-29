#include "xl6_der_internal.h"
#include "xl6_text_thread.h"
typedef struct Builder {XL6_TextThreadOwner owner;xbuffer output;size_t limit;} Builder;
static Builder* check(void* raw)
{Builder* self=raw;if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return xl6TextThreadCheck(&self->owner)?self:NULL;}
void xl6DerBuilderDestroyV1(void* raw)
{
    Builder* self=raw;if (!self) return;xerror* error=xrtTakeError();xl6TextThreadDrop(&self->owner);
    xl6CryptoFreeSecret(self->output.Data,self->output.Capacity);xl6CryptoFreeSecret(self,sizeof(*self));xrtClearError();xrtSetErrorTake(error);
}
void* xl6DerBuilderCreateV1(int64_t limit)
{
    size_t size;if (!xl6CryptoLength(limit,INT64_MAX,&size)) return NULL;
    Builder* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    self->limit=size;if (!xl6TextThreadInit(&self->owner)) {xl6DerBuilderDestroyV1(self);return NULL;}return self;
}
static void commit(Builder* self,size_t before,bool ok)
{
    /* XRT appenders never publish a partial TLV. Append in place, then enforce
     * the language output budget without a temporary allocation per field.
     * Validation failure also rolls back just the uncommitted suffix. */
    if (ok && self->output.Size>self->limit) {xrtSetErrorKind(XERR_RANGE);ok=false;}
    if (!ok && self->output.Size>before) {
        xrtSecureZero(self->output.Data+before,self->output.Size-before);
        self->output.Size=before;
    }
}
void xl6DerBuilderBooleanV1(void* raw,bool value)
{Builder* self=check(raw);if (!self) return;size_t before=self->output.Size;commit(self,before,xrtDerAppendBoolean(&self->output,value));}
void xl6DerBuilderInt64V1(void* raw,int64_t value)
{Builder* self=check(raw);if (!self) return;size_t before=self->output.Size;commit(self,before,xrtDerAppendInt64(&self->output,value));}
void xl6DerBuilderUInt64V1(void* raw,uint64_t value)
{Builder* self=check(raw);if (!self) return;size_t before=self->output.Size;commit(self,before,xrtDerAppendUInt64(&self->output,value));}
void xl6DerBuilderNullV1(void* raw)
{Builder* self=check(raw);if (!self) return;size_t before=self->output.Size;commit(self,before,xrtDerAppendNull(&self->output));}
void xl6DerBuilderOidV1(void* raw,xstrview text,int64_t size)
{
    Builder* self=check(raw);if (!self || !xl6TextSelectRaw(&text,size)) return;
    /* Dotted text can be longer than its DER encoding. Only the native
     * encoded result, not the source character count, consumes output budget. */
    size_t before=self->output.Size;commit(self,before,xrtDerAppendOid(&self->output,text));
}
void xl6DerBuilderTagV1(void* raw,int64_t tagClass,int64_t number,bool constructed,const xvalue* value,int64_t size)
{
    Builder* self=check(raw);xbytesview input;
    if (!self || !xl6BufferBytesInput(value,size,&input)) return;
    if (tagClass<0 || tagClass>3 || number<0 || (uint64_t)number>UINT32_MAX || input.Size>self->limit-self->output.Size) {xrtSetErrorKind(XERR_RANGE);return;}
    size_t before=self->output.Size;
    bool ok=xrtDerAppend(&self->output,(xasn1class)tagClass,(uint32)number,constructed,input);
    /* Arbitrary TLVs are still canonical DER, including all nested children. */
    if (ok && self->output.Size<=self->limit)
        ok=xrtDerValidate(self->output.Data+before,self->output.Size-before);
    commit(self,before,ok);
}
void xl6DerBuilderBitsV1(void* raw,const xvalue* value,int64_t unused,int64_t size)
{
    Builder* self=check(raw);xbytesview input;
    if (!self || !xl6BufferBytesInput(value,size,&input)) return;
    if (unused<0 || unused>7 || input.Size>self->limit-self->output.Size) {xrtSetErrorKind(XERR_RANGE);return;}
    size_t before=self->output.Size;commit(self,before,xrtDerAppendBitString(&self->output,input,(uint8)unused));
}
void xl6DerBuilderRawV1(void* raw,const xvalue* value,int64_t size)
{
    Builder* self=check(raw);xbytesview input;
    if (!self || !xl6BufferBytesInput(value,size,&input)) return;
    if (input.Size>self->limit-self->output.Size) {xrtSetErrorKind(XERR_RANGE);return;}
    if (xrtDerValidate(input.Data,input.Size)) xrtBufferAppend(&self->output,input);
}
xvalue* xl6DerBuilderFinishV1(void* raw)
{Builder* self=check(raw);return self?xrtValueBytes(xrtBufferView(&self->output)):NULL;}
void xl6DerBuilderResetV1(void* raw)
{Builder* self=check(raw);if (self) {xrtSecureZero(self->output.Data,self->output.Size);self->output.Size=0;}}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6DerBuilderHandlePolicyV1Get,xl6DerBuilderDestroyV1)
