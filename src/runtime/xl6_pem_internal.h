#ifndef XL6_PEM_INTERNAL_H
#define XL6_PEM_INTERNAL_H
#include "xl6_der_internal.h"
typedef struct XL6_PemBlock {str text;xpemblock block;size_t limit;} XL6_PemBlock;
static inline bool xl6PemLabel(xstrview value,str* result)
{
    if (!xl6TextSelectRaw(&value,-1)) return false;
    if (memchr(value.Data,0,value.Size)) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    *result=xrtStrDupView(value);return *result!=NULL;
}
static inline XL6_PemBlock* xl6PemCopy(const xpemblock* block,size_t limit)
{
    XL6_PemBlock* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    self->limit=limit;self->text=xrtStrDupView(block->Raw);
    if (!self->text) {xl6CryptoFreeSecret(self,sizeof(*self));return NULL;}
    xpemcursor cursor;
    if (!xrtPemInit(&cursor,self->text,block->Raw.Size) || xrtPemRead(&cursor,&self->block)!=XPEM_BLOCK) {
        xl6CryptoFreeSecret(self->text,block->Raw.Size);xl6CryptoFreeSecret(self,sizeof(*self));return NULL;
    }
    return self;
}
#endif
