#ifndef XL6_LOG_WRITER_H
#define XL6_LOG_WRITER_H
#include "xl6_log_record.h"
#include "xl6_io_handle.h"
#include <limits.h>
static bool log_writer_write(xbytesview data,void* context)
{
    xlir_io_writer* writer=context;
    if (!writer || !writer->stream) { xrtSetErrorKind(XERR_CLOSED); return false; }
    size_t done=0;
    return xrtWriterWriteFull(writer->stream,data.Data,data.Size,&done);
}
#endif
