#ifndef XL6_PROCESS_TEXT_H
#define XL6_PROCESS_TEXT_H
#include "xl6_text_internal.h"
#include <string.h>

/* Exact language view -> owned OS text. Selection is allowed to exclude a NUL
 * suffix but must not split a UTF-8 scalar in the selected prefix. */
static inline char* xl6ProcessCString(xstrview value,int64_t size,bool nonempty)
{
    if (!xl6TextSelect(&value,size)) return NULL;
    if ((nonempty && !value.Size) || memchr(value.Data,0,value.Size)) {
        xrtSetErrorInfo(XERR_VALUE,"xrt.process",XPROCESS_ERROR_COMMAND,
            "OS process text must be UTF-8 without embedded NUL");
        return NULL;
    }
    return xrtStrDupView(value);
}
#endif
