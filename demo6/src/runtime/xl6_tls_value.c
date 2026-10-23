#include "xl6_tls_internal.h"

bool xl6TlsFailV1(xerrkind kind,const char* message)
{
    if(!xrtGetError()){
        xerror* error=xrtErrorCreate(kind,"xlang.tls",1,message);
        if(error)xrtSetErrorTake(error);
        else if(!xrtGetError())xrtSetErrorKind(XERR_MEMORY);
    }
    return false;
}
bool xl6TlsOutcomeV1(bool ok)
{
    if(!ok && !xrtGetError())xl6TlsFailV1(XERR_STATE,"TLS operation failed without a diagnostic");
    return ok && !xrtGetError();
}
bool xl6TlsLimitV1(size_t actual,int64_t limit,const char* message)
{
    if(limit<0)return xl6TlsFailV1(XERR_RANGE,"negative TLS budget");
    return !limit || actual<=(uint64_t)limit || xl6TlsFailV1(XERR_RANGE,message);
}
