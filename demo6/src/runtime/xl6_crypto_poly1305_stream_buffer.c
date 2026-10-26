#include "xl6_crypto_poly1305_stream_internal.h"
#include "xl6_crypto_abi.h"
void xl6CryptoPoly1305UpdateBufferV1(void* raw,void* buffer,int64_t size)
{
    XL6_Poly1305State* self=xl6CryptoPoly1305Checked(raw);xbytesview input;
    if (!self || !xl6BufferInput(buffer,size,&input)) return;
    (void)xrtPoly1305Update(&self->state,input.Data,input.Size);
}
