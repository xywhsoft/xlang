#ifndef XL6_VFS_PRIVATE_H
#define XL6_VFS_PRIVATE_H
#include "xl6_text_value.h"
#include <string.h>
/* Language Vfs owns a private namespace, not a borrowed/raw host xvfs. Only
 * the three resident builtin mount factories can populate this namespace.
 * A host's arbitrary provider/context is not admitted by this resource ABI. */
typedef struct XL6_VfsOwner { xvfs native; } XL6_VfsOwner;
static inline xvfs xl6VfsGet(void* handle)
{
    XL6_VfsOwner* owner=handle;
    if(!owner || !owner->native){xrtSetErrorKind(XERR_CLOSED);return NULL;}
    return owner->native;
}
/* Managed string ABI supplies a trailing NUL. Reject embedded NUL over its
 * complete logical length before lending it to a native path API. */
static inline const char* xl6VfsPath(xstrview path)
{
    if(!path.Data){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(memchr(path.Data,0,path.Size)){xrtSetErrorKind(XERR_VALUE);return NULL;}
    return path.Data;
}
#endif
