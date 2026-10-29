#include "src/runtime/xl6_log_file_options.h"
void* xl6LogTextFileV1(xstrview path,int64_t level,int64_t mode,int64_t sync,
    uint64_t maxBytes,int64_t backups,int64_t recordLimit,int64_t bufferLimit,
    uint64_t interval,int64_t style,int64_t flags,int64_t offset)
{
    xlogfileoptions options; xlogtextconfig text; char* owned;
    if (!xl6LogTextConfig(&text,style,flags,offset) ||
        !log_file_options(&options,path,&owned,level,mode,sync,maxBytes,backups,recordLimit,bufferLimit,interval)) return NULL;
    void* sink=xrtLogTextFile(&options,&text);
    xrtFree(owned); return sink;
}
