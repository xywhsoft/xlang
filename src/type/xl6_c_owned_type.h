#ifndef XL6_C_OWNED_TYPE_H
#define XL6_C_OWNED_TYPE_H
#include <string.h>
/* These are normalization rules, not physical aliases or runtime inheritance. */
static inline const char* xl6COwnedBaseName(const char* name)
{
    if (name && !strcmp(name, "cstring")) return "string";
    if (name && !strcmp(name, "cbytes")) return "bytes";
    return NULL;
}
#endif
