#include "xl6_text_internal.h"
#include "xl6_foundation_abi.h"

typedef struct ReportOutput { xbuffer buffer; size_t limit; } ReportOutput;
static bool write_report(xbytesview bytes, void* user)
{
    ReportOutput* out = user;
    if (bytes.Size > out->limit - out->buffer.Size) { xrtSetErrorKind(XERR_RANGE); return false; }
    return xrtBufferAppend(&out->buffer, bytes);
}
static xstrview report(xmemdebugreportformat format, int64_t limit)
{
    if (limit < 0 || (uint64_t)limit >= SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    ReportOutput out = {{0},(size_t)limit};
    if (!xrtMemDebugReport(format,write_report,&out) || !xrtBufferAppendByte(&out.buffer,0)) {
        xerror* failure = xrtTakeError(); xrtBufferUnit(&out.buffer);
        xrtSetErrorTake(failure);
        if (xrtErrorKind(xrtGetError()) == XERR_NONE) xrtSetErrorKind(XERR_MEMORY);
        return xl6TextFailure;
    }
    size_t size = out.buffer.Size - 1;
    char* data = (char*)xrtBufferTake(&out.buffer,NULL,NULL);
    /* Native allocation-site text can originate in a foreign C producer.
     * Never publish invalid UTF-8 as a language string. */
    if (xrtUtf8Count((xstrview){data,size}) == XRT_NPOS) {
        xrtFree(data);
        if (xrtErrorKind(xrtGetError()) == XERR_NONE) xrtSetErrorKind(XERR_VALUE);
        return xl6TextFailure;
    }
    return xl6TextOwn(data,size);
}
xstrview xl6MemoryReportTextV1(int64_t limit) { return report(XMEMDEBUG_REPORT_TEXT,limit); }
xstrview xl6MemoryReportJsonV1(int64_t limit) { return report(XMEMDEBUG_REPORT_JSON,limit); }
