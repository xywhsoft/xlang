#ifndef XL6_NET_BYTES_INTERNAL_H
#define XL6_NET_BYTES_INTERNAL_H
#include "xl6_buffer_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
static inline void xl6NetBytesFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static inline bool xl6NetBytesRangeV1(const xnetbytes* value,int64_t size,int64_t offset,xbytesview* view)
{
    if(!value){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    *view=xrtNetBytesView(value);
    if(offset<0||(uint64_t)offset>view->Size){xrtSetErrorKind(XERR_RANGE);return false;}
    if(view->Data)view->Data+=(size_t)offset;
    view->Size-=(size_t)offset;
    return xl6BufferSelect(view,size);
}
#endif
