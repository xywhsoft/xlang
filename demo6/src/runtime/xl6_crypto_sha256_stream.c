#include "xl6_crypto_internal.h"
#include "xl6_text_thread.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
typedef struct State {XL6_TextThreadOwner owner;xsha256 state,initial;} State;
static State* check(void* raw)
{State* self=raw;if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return xl6TextThreadCheck(&self->owner)?self:NULL;}
void xl6CryptoSha256DestroyV1(void* raw)
{State* self=raw;if (!self) return;xerror* error=xrtTakeError();xl6TextThreadDrop(&self->owner);xrtSecureZero(self,sizeof(*self));xrtFree(self);xrtClearError();xrtSetErrorTake(error);}
void* xl6CryptoSha256CreateV1(void)
{
    xbytesview key={0};
    State* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    if (!xl6TextThreadInit(&self->owner)) {xl6CryptoSha256DestroyV1(self);return NULL;}
    xrtSha256Init(&self->state);
    self->initial=self->state;return self;
}
void* xl6CryptoSha256CloneV1(void* raw)
{State* self=check(raw);if (!self) return NULL;State* copy=xrtCalloc(1,sizeof(*copy));if (!copy) return NULL;
 if (!xl6TextThreadInit(&copy->owner)) {xl6CryptoSha256DestroyV1(copy);return NULL;}copy->state=self->state;copy->initial=self->initial;return copy;}
void xl6CryptoSha256ResetV1(void* raw)
{State* self=check(raw);if (self) self->state=self->initial;}
void xl6CryptoSha256UpdateTextV1(void* raw,xstrview text,int64_t size)
{State* self=check(raw);xbytesview input={0};if (!self || !xl6TextSelectRaw(&text,size)) return;
 input=(xbytesview){(const uint8*)text.Data,text.Size};
 (void)xrtSha256Update(&self->state,input.Data,input.Size);}
void xl6CryptoSha256UpdateBytesV1(void* raw,const xvalue* data,int64_t size)
{State* self=check(raw);xbytesview input={0};if (!self || !xl6BufferBytesInput(data,size,&input)) return;

 (void)xrtSha256Update(&self->state,input.Data,input.Size);}
void xl6CryptoSha256UpdateBufferV1(void* raw,void* data,int64_t size)
{State* self=check(raw);xbytesview input={0};if (!self || !xl6BufferInput(data,size,&input)) return;

 (void)xrtSha256Update(&self->state,input.Data,input.Size);}
xvalue* xl6CryptoSha256FinalV1(void* raw)
{State* self=check(raw);if (!self) return NULL;uint8 output[32];
 bool ok=xrtSha256Final(&self->state,output);
 xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
 xrtSecureZero(output,sizeof(output));return result;}
xstrview xl6CryptoSha256HexV1(void* raw)
{State* self=check(raw);if (!self) return xl6TextFailure;uint8 output[32];
 bool ok=xrtSha256Final(&self->state,output);
 xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
 xrtSecureZero(output,sizeof(output));return result;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoSha256HandlePolicyV1Get,xl6CryptoSha256DestroyV1)
