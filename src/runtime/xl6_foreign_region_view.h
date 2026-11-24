#ifndef XL6_FOREIGN_REGION_VIEW_H
#define XL6_FOREIGN_REGION_VIEW_H
#include <limits.h>
#include <stdint.h>
#include <string.h>
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xrt_decl.h"
#endif

/* Stack descriptors for caller-owned row storage. Only active row bytes
 * belong to the scoped view. Padding is never returned or written. */
typedef struct XL6_ReadableRegionViewV1 {
    const uint8_t* Data;
    size_t RowBytes, RowCount, Stride;
} XL6_ReadableRegionViewV1;

typedef struct XL6_WritableRegionViewV1 {
    uint8_t* Data;
    size_t RowBytes, RowCount, Stride;
} XL6_WritableRegionViewV1;

static inline bool xl6ForeignCallbackRegionLayoutV1(int width,int height,int stride,size_t unit,
    size_t* rowBytes,size_t* rowCount,size_t* effectiveStride)
{
    if(width<=0 || height<=0 || !unit || (size_t)width>SIZE_MAX/unit){
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    const size_t bytes=(size_t)width*unit;
    const size_t rows=(size_t)height;
    const size_t step=stride>0 ? (size_t)stride:bytes;
    if(bytes>(size_t)INT_MAX || step<bytes || rows-1>(SIZE_MAX-bytes)/step){
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    *rowBytes=bytes;
    *rowCount=rows;
    *effectiveStride=step;
    return true;
}

static inline int xl6ForeignReadableRegionRowV1(const XL6_ReadableRegionViewV1* region,size_t row,xbytesview* output)
{
    if(!region || !region->Data || !output){
        xrtSetErrorKind(XERR_ARGUMENT);
        return -1;
    }
    if(row>=region->RowCount){
        xrtSetErrorKind(XERR_RANGE);
        return -1;
    }
    const xbytesview view={region->Data+row*region->Stride,region->RowBytes};
    *output=view;
    return 0;
}

static inline int xl6ForeignReadableRegionAtV1(const XL6_ReadableRegionViewV1* region,size_t row,size_t offset,uint8_t* output)
{
    xbytesview view={0};
    if(!output){
        xrtSetErrorKind(XERR_ARGUMENT);
        return -1;
    }
    if(xl6ForeignReadableRegionRowV1(region,row,&view)!=0)return -1;
    if(offset>=view.Size){
        xrtSetErrorKind(XERR_RANGE);
        return -1;
    }
    *output=view.Data[offset];
    return 0;
}

static inline bool xl6ForeignWritableRegionWriteV1(const XL6_WritableRegionViewV1* region,
    const void* source,size_t length,size_t row,size_t offset)
{
    if(!region || !region->Data || (length && !source)){
        xrtSetErrorKind(XERR_ARGUMENT);
        return false;
    }
    if(row>=region->RowCount || offset>region->RowBytes || length>region->RowBytes-offset){
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    if(length)memmove(region->Data+row*region->Stride+offset,source,length);
    return true;
}
#endif
