#include "xl6_x509_records_internal.h"

void* xl6X509RecordAllocV1(size_t header,size_t count,size_t itemSize,size_t alignment,
    xbytesview source,void** items,xbytesview* owned)
{
    if(!owned || !header || !alignment || (alignment&(alignment-1)) ||
        alignment>_Alignof(xx509nameattr) || (count && (!items || !itemSize)) || (source.Size && !source.Data)) {
        xl6X509FailV1(XERR_STATE,"invalid X.509 record allocation descriptor");return NULL;
    }
    if(source.Size>INT64_MAX || header>SIZE_MAX-(alignment-1)) {
        xl6X509FailV1(XERR_RANGE,"X.509 record size overflow");return NULL;
    }
    size_t offset=(header+alignment-1)&~(alignment-1);
    if((count && itemSize>(SIZE_MAX-offset)/count) || source.Size>SIZE_MAX-offset-count*itemSize) {
        xl6X509FailV1(XERR_RANGE,"X.509 index allocation size overflow");return NULL;
    }
    size_t bytes=offset+count*itemSize;
    void* result=xrtCalloc(1,bytes+source.Size);if(!result)return NULL;
    uint8* data=(uint8*)result+bytes;
    if(source.Size)memcpy(data,source.Data,source.Size);
    if(items)*items=(uint8*)result+offset;
    *owned=(xbytesview){source.Size?data:NULL,source.Size};return result;
}
bool xl6X509RebaseV1(xbytesview source,xbytesview owned,xbytesview field,xbytesview* output)
{
    if(!output || source.Size!=owned.Size || (source.Size && (!source.Data || !owned.Data)))
        return xl6X509FailV1(XERR_STATE,"invalid X.509 record relocation");
    if(!field.Data) {
        if(field.Size)return xl6X509FailV1(XERR_STATE,"missing X.509 field storage");
        *output=(xbytesview){0};return true;
    }
    uintptr_t base=(uintptr_t)source.Data,address=(uintptr_t)field.Data;
    if(!source.Data || !owned.Data || address<base || address-base>source.Size || field.Size>source.Size-(size_t)(address-base))
        return xl6X509FailV1(XERR_STATE,"X.509 field lies outside its record");
    *output=(xbytesview){owned.Data+(size_t)(address-base),field.Size};return true;
}
bool xl6X509IndexV1(int64_t index,size_t count,size_t* selected)
{
    if(!selected)return xl6X509FailV1(XERR_STATE,"missing X.509 record index output");
    if(index<0 || (uint64_t)index>=count)return xl6X509FailV1(XERR_RANGE,"X.509 record index out of range");
    *selected=(size_t)index;return true;
}
void xl6X509BufferFreeV1(xbuffer* buffer)
{
    xerror* prior=xrtTakeError();xrtBufferUnit(buffer);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
