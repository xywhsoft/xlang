#ifndef XL6_DER_INTERNAL_H
#define XL6_DER_INTERNAL_H
#include "xl6_crypto_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Values and cursors copy their complete source. Native interior views never
 * survive their owner and never cross the language boundary as a pointer. */
typedef struct XL6_DerValue {bytes data;xdervalue value;} XL6_DerValue;
static inline bool xl6DerBound(xbytesview input,int64_t limit)
{
    if (limit<0 || input.Size>(uint64_t)limit) {xrtSetErrorKind(XERR_RANGE);return false;}
    return true;
}
static inline XL6_DerValue* xl6DerChecked(void* raw)
{
    if (!raw) xrtSetErrorKind(XERR_ARGUMENT);
    return raw;
}
/* The source is validated before allocation; this function owns no input. */
static inline XL6_DerValue* xl6DerCopyValue(xbytesview raw)
{
    if (!xrtDerValidate(raw.Data,raw.Size)) return NULL;
    XL6_DerValue* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    self->data=xrtMalloc(raw.Size?raw.Size:1);
    if (!self->data) {xl6CryptoFreeSecret(self,sizeof(*self));return NULL;}
    memcpy(self->data,raw.Data,raw.Size);
    xdercursor cursor;
    if (!xrtDerInit(&cursor,self->data,raw.Size) || xrtDerRead(&cursor,&self->value)!=XDER_VALUE) {
        xl6CryptoFreeSecret(self->data,raw.Size);xl6CryptoFreeSecret(self,sizeof(*self));return NULL;
    }
    return self;
}
#endif
