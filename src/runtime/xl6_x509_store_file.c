#include "xl6_x509_store_input.h"

int64_t xl6X509StoreAddFileV1(void* raw,xstrview path,int64_t limit)
{
    if(xrtGetError())return 0;
    size_t bound=0,size=0;
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing trust store");return 0;}
    const char* name=xl6X509StoreInputPathV1(path);
    if(!name || !xl6X509StoreInputLimitV1(limit,&bound))return 0;
    bytes data=xrtFileReadAllLimit(name,bound,&size);
    return xl6X509StoreInputTakeV1(raw,data,size);
}
