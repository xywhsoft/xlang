#ifndef XL6_C_OUTPUT_BYTES_H
#define XL6_C_OUTPUT_BYTES_H
/* Called exactly once after a native producer, even on status failure. Data
 * must be an independent allocation from the exact resident XRT allocator.
 * Admission transfers the payload without copying; the xvalue shell can fail.
 * NULL/0 is valid empty bytes. A non-NULL empty allocation is still released. */
static inline xvalue* xl6COutputBytesTakeV1(void* data,uint64_t size,bool accepted)
{
    bytes owned=(bytes)data;
    xvalue* result=NULL;
    if(accepted){
        if(size>INT64_MAX || size>SIZE_MAX)xrtSetErrorKind(XERR_RANGE);
        else if(!owned && size)xrtSetErrorKind(XERR_STATE);
        else result=xrtValueBytesTake(&owned,(size_t)size);
    }
    xerror* error=xrtTakeError();
    xrtFree(owned);
    xrtClearError();xrtSetErrorTake(error);
    return result;
}
#endif
