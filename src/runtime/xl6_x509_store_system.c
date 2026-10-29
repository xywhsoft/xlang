#include "xl6_x509_store_internal.h"

void* xl6X509StoreSnapshotSystemV1(void)
{
    if(xrtGetError())return NULL;
    xx509store* value=xrtX509StoreSystem();if(!value)return NULL;
    size_t count=xrtX509StoreCount(value);int64_t size=0;
    for(size_t i=0;i<count;++i){
        size_t bytes=xrtX509StoreCertificate(value,i)->Raw.Size;
        if(bytes>(uint64_t)(INT64_MAX-size)){
            xl6X509FailV1(XERR_RANGE,"system trust source exceeds representable bytes");
            xerror* error=xrtTakeError();xrtX509StoreFree(value);xrtSetErrorTake(error);return NULL;
        }
        size+=(int64_t)bytes;
    }
    return xl6X509StoreVersionTakeV1(&value,size);
}
