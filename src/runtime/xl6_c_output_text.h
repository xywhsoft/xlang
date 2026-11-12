#ifndef XL6_C_OUTPUT_TEXT_H
#define XL6_C_OUTPUT_TEXT_H
#include "xl6_error_text.h"
#include <limits.h>
/* The producer promises an independent resident-XRT allocation of size+1
 * readable bytes, with a terminator at size. Interior NULs remain ordinary
 * UTF-8 scalar U+0000. This consumes the allocation on every path, writes the
 * exact view only on success, and never infers success from a diagnostic. */
static inline bool xl6COutputTextTakeV1(void* data,uint64_t size,bool accepted,
    int line,int column,xstrview* output)
{
    char* owned=(char*)data;
    bool ok=false;
    *output=(xstrview){NULL,0};
    if(accepted && !xrtGetError()) {
        if(size>INT64_MAX || size>=SIZE_MAX)xrtSetErrorKind(XERR_RANGE);
        else if(!owned && size)xrtSetErrorKind(XERR_STATE);
        else if(owned && owned[size]!=0)xrtSetErrorKind(XERR_VALUE);
        else {
            xstrview text={owned?owned:"",(size_t)size};
            if(xrtUtf8Count(text)==XRT_NPOS)
                xrtSetErrorTake(xl6ErrorCreateV1("EncodingError",
                    xrtStrView("native output is not valid UTF-8"),line,column));
            else {*output=(xstrview){owned,(size_t)size};owned=NULL;ok=true;}
        }
    }
    xerror* error=xrtTakeError();
    xrtFree(owned);
    xrtClearError();xrtSetErrorTake(error);
    return ok;
}
#endif
