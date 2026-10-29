#ifndef XL6_X509_STORE_INPUT_H
#define XL6_X509_STORE_INPUT_H
#include "xl6_x509_store_internal.h"
#include <string.h>

/* Managed strings provide a trailing NUL; never allow a logical path suffix
 * after an embedded NUL to disappear at the native file boundary. */
static inline const char* xl6X509StoreInputPathV1(xstrview path)
{
    if(!path.Data){xl6X509FailV1(XERR_ARGUMENT,"missing trust source path");return NULL;}
    if(memchr(path.Data,0,path.Size)){xl6X509FailV1(XERR_VALUE,"embedded NUL in trust source path");return NULL;}
    return path.Data;
}

/* Reader limit, not a truncating prefix: zero permits no bytes. File/VFS IO
 * always finishes before acquiring the mutable store's private lock. */
static inline bool xl6X509StoreInputLimitV1(int64_t limit,size_t* result)
{
    if(limit<0 || (uint64_t)limit>SIZE_MAX-1u)
        return xl6X509FailV1(XERR_RANGE,"invalid trust source input limit");
    *result=(size_t)limit;return true;
}
static inline int64_t xl6X509StoreInputTakeV1(void* raw,bytes data,size_t size)
{
    if(!data)return 0;
    int64_t added=xl6X509StoreAddDataSpanV1(raw,(xbytesview){data,size});
    xl6X509FreeV1(data);return added;
}
#endif
