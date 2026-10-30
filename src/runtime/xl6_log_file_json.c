#include "src/runtime/xl6_log_file_options.h"
void* xl6LogJsonFileV1(xstrview path,int64_t level,int64_t mode,int64_t sync,
    uint64_t maxBytes,int64_t backups,int64_t recordLimit,int64_t bufferLimit,
    uint64_t interval,int64_t flags,int64_t escape,int64_t fields,int64_t nonfinite,int64_t depth)
{
    xlogfileoptions options; xlogjsonconfig json; char* owned;
    if (!xl6LogJsonConfig(&json,flags,escape,fields,nonfinite,depth) ||
        !log_file_options(&options,path,&owned,level,mode,sync,maxBytes,backups,recordLimit,bufferLimit,interval)) return NULL;
    void* sink=xrtLogJsonFile(&options,&json);
    xrtFree(owned); return xl6LogSinkBindNativeV1(sink);
}
