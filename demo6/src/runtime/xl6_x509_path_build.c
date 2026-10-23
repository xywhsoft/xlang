#include "xl6_x509_path_build_internal.h"
void* xl6X509PathBuildV1(XL6_NativeClassArrayContextV1 context,void* target,const xvalue* candidates,void* trust,void* policy,
    int64_t maxCandidates,int64_t maxSourceBytes)
{return xl6X509PathBuildArrayRunV1(context,target,candidates,trust,policy,maxCandidates,maxSourceBytes,NULL,NULL);}
