#include "src/runtime/xl6_log_writer.h"
int64_t xl6LogTextWriterV1(void* handle,void* writer,int64_t style,int64_t flags,int64_t offset)
{
    xlogtextconfig config; size_t written=0;
    if (!handle || !writer) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    if (!xl6LogTextConfig(&config,style,flags,offset) ||
        !xrtLogTextWrite(&((xl6_log_record*)handle)->record,&config,log_writer_write,writer,&written)) return -1;
    if (written>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return -1; }
    return (int64_t)written;
}
