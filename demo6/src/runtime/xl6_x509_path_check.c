#include "xl6_x509_path_array_internal.h"
void* xl6X509PathCheckV1(XL6_NativeClassArrayContextV1 context,const xvalue* array,void* anchor,void* policy)
{return xl6X509PathArrayRunV1(context,array,anchor,policy,NULL,NULL);}
