#include "xl6_x509_path_build_internal.h"
#include "xl6_x509_path_critical_internal.h"
void* xl6X509PathBuildCallbackV1(XL6_NativeClassArrayContextV1 input,void* target,const xvalue* array,void* trust,void* policy,
    int64_t maxCandidates,int64_t maxSourceBytes,void* raw,XL6_Closure* callback)
{
    XL6_X509PathDispatchV1 call;
    if(!xl6X509PathDispatchBeginV1(&call,raw,callback))return NULL;
    void* result=xl6X509PathBuildArrayRunV1(input,target,array,trust,policy,maxCandidates,maxSourceBytes,xl6X509PathDispatchCriticalV1,&call);
    xl6X509PathDispatchEndV1(&call);return result;
}
