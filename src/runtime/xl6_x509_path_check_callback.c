#include "xl6_x509_path_critical_internal.h"
void* xl6X509PathCheckCallbackV1(XL6_NativeClassArrayContextV1 input,const xvalue* array,void* anchor,void* policy,void* raw,XL6_Closure* callback)
{
    XL6_X509PathDispatchV1 call;
    if(!xl6X509PathDispatchBeginV1(&call,raw,callback))return NULL;
    void* result=xl6X509PathArrayRunV1(input,array,anchor,policy,xl6X509PathDispatchCriticalV1,&call);
    xl6X509PathDispatchEndV1(&call);return result;
}
