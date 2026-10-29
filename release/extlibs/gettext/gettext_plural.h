#ifndef XL_GETTEXT_PLURAL_H
#define XL_GETTEXT_PLURAL_H

#include <stddef.h>
#include <stdint.h>
typedef struct xvalue xvalue;

int xgt_plural_index(xvalue *catalog, int64_t count, size_t *index);
int xgt_plural_validate(xvalue *catalog);

#endif
