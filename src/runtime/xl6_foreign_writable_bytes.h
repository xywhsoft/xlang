#ifndef XL6_FOREIGN_WRITABLE_BYTES_H
#define XL6_FOREIGN_WRITABLE_BYTES_H
#include "xl6_foreign_scope_view.h"
#include <string.h>
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xrt_decl.h"
#endif

static inline bool xl6ForeignWritableBytesWriteV1(const XL6_WritableBytesViewV1* output,
    const void* source,size_t size,size_t offset)
{
    if(!output || (size && !source) || (output->Size && !output->Data)){
        xrtSetErrorKind(XERR_ARGUMENT);
        return false;
    }
    if(offset>output->Size || size>output->Size-offset){
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    if(size)memmove(output->Data+offset,source,size);
    return true;
}
#endif
