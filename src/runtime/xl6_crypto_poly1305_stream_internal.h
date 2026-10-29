#ifndef XL6_CRYPTO_POLY1305_STREAM_INTERNAL_H
#define XL6_CRYPTO_POLY1305_STREAM_INTERNAL_H
#include "xl6_crypto_internal.h"
#include "xl6_text_thread.h"
typedef struct XL6_Poly1305State {XL6_TextThreadOwner owner;xpoly1305 state;} XL6_Poly1305State;
static inline XL6_Poly1305State* xl6CryptoPoly1305Checked(void* raw)
{
    XL6_Poly1305State* self=raw;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6TextThreadCheck(&self->owner)?self:NULL;
}
#endif
