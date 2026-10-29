#include "xl6_crypto_poly1305_stream_internal.h"
#include "xl6_crypto_abi.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

typedef XL6_Poly1305State State;
void xl6CryptoPoly1305DestroyV1(void* raw)
{
    State* self=raw;if (!self) return;
    xerror* error=xrtTakeError();xl6TextThreadDrop(&self->owner);
    xl6CryptoFreeSecret(self,sizeof(*self));xrtClearError();xrtSetErrorTake(error);
}
void* xl6CryptoPoly1305CreateSpanV1(xbytesview key)
{
    if (!xl6BufferSpanInput(key,-1,&key)) return NULL;
    if (key.Size!=32) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    State* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    if (!xl6TextThreadInit(&self->owner) || !xrtPoly1305Init(&self->state,key.Data)) {
        xl6CryptoPoly1305DestroyV1(self);return NULL;
    }
    return self;
}
void* xl6CryptoPoly1305CreateV1(const xvalue* keyValue)
{xbytesview key;return xl6CryptoMaterial(keyValue,&key)?xl6CryptoPoly1305CreateSpanV1(key):NULL;}
/* Reset requires an explicit NEW one-time key, never a saved initial key. */
void xl6CryptoPoly1305ResetSpanV1(void* raw,xbytesview key)
{
    State* self=xl6CryptoPoly1305Checked(raw);if (!self || !xl6BufferSpanInput(key,-1,&key)) return;
    if (key.Size!=32) {xrtSetErrorKind(XERR_ARGUMENT);return;}
    (void)xrtPoly1305Init(&self->state,key.Data);
}
void xl6CryptoPoly1305ResetV1(void* raw,const xvalue* keyValue)
{xbytesview key;if (xl6CryptoMaterial(keyValue,&key)) xl6CryptoPoly1305ResetSpanV1(raw,key);}
void xl6CryptoPoly1305UpdateTextV1(void* raw,xstrview text,int64_t size)
{
    State* self=xl6CryptoPoly1305Checked(raw);if (!self || !xl6TextSelectRaw(&text,size)) return;
    (void)xrtPoly1305Update(&self->state,text.Data,text.Size);
}
void xl6CryptoPoly1305UpdateBytesSpanV1(void* raw,xbytesview input,int64_t size)
{
    State* self=xl6CryptoPoly1305Checked(raw);if (!self || !xl6BufferSpanInput(input,size,&input)) return;
    (void)xrtPoly1305Update(&self->state,input.Data,input.Size);
}
void xl6CryptoPoly1305UpdateBytesV1(void* raw,const xvalue* value,int64_t size)
{
    State* self=xl6CryptoPoly1305Checked(raw);xbytesview input;
    if (!self || !xl6BufferBytesInput(value,size,&input)) return;
    (void)xrtPoly1305Update(&self->state,input.Data,input.Size);
}
/* Exactly XRT Final: a non-destructive snapshot, not a new independent MAC.
 * Never authenticate distinct messages/prefixes with the same one-time key. */
xvalue* xl6CryptoPoly1305FinalV1(void* raw)
{
    State* self=xl6CryptoPoly1305Checked(raw);if (!self) return NULL;
    uint8 output[16];bool ok=xrtPoly1305Final(&self->state,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoPoly1305HandlePolicyV1Get,xl6CryptoPoly1305DestroyV1)
