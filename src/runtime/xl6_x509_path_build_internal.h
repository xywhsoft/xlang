#ifndef XL6_X509_PATH_BUILD_INTERNAL_H
#define XL6_X509_PATH_BUILD_INTERNAL_H
#include "xl6_x509_store_internal.h"
#include "xl6_x509_path_array_internal.h"
void* xl6X509PathBuildCreateV1(void*,void* const*,size_t,void*,const xx509pathconfig*,int64_t,int64_t,int64_t,int64_t,int64_t);
void* xl6X509PathBuildArrayRunV1(XL6_NativeClassArrayContextV1,void*,const xvalue*,void*,void*,int64_t,int64_t,xx509criticalproc,void*);
#endif
