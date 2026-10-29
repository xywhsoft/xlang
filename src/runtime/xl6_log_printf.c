#include "src/runtime/xl6_log_record.h"
#include "src/common/xl6_printf_contract.h"
#include <stdarg.h>
static char* printf_format(xstrview format)
{
    if (!xl6PrintfContract((const unsigned char*)format.Data,format.Size,NULL,NULL)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    return xrtStrDupN(format.Data,format.Size);
}
int64_t xl6LogPrintfV1(void* logger,int64_t level,xstrview format,...)
{
    if (!logger) { xrtSetErrorKind(XERR_ARGUMENT); return XLOG_RESULT_ERROR; }
    if (level<0 || level>5) { xrtSetErrorKind(XERR_RANGE); return XLOG_RESULT_ERROR; }
    char* text=printf_format(format); if (!text) return XLOG_RESULT_ERROR;
    va_list args; va_start(args,format);
    xlogresult result=xrtLogPrintfV(logger,(xloglevel)level,text,args);
    va_end(args); xrtFree(text); return result;
}
int64_t xl6LogDefaultPrintfV1(int64_t level,xstrview format,...)
{
    if (level<0 || level>5) { xrtSetErrorKind(XERR_RANGE); return XLOG_RESULT_ERROR; }
    char* text=printf_format(format); if (!text) return XLOG_RESULT_ERROR;
    xlogger* logger=xrtLogDefault();
    if (!logger) { xrtFree(text); return XLOG_RESULT_SKIPPED; }
    va_list args; va_start(args,format);
    xlogresult result=xrtLogPrintfV(logger,(xloglevel)level,text,args);
    va_end(args); xrtFree(text); xrtLogFree(logger); return result;
}
int64_t xl6LogRecordPrintfV1(void* logger,void* handle,xstrview format,...)
{
    if (!logger || !handle) { xrtSetErrorKind(XERR_ARGUMENT); return XLOG_RESULT_ERROR; }
    const xlogrecord* record=&((xl6_log_record*)handle)->record;
    char* text=printf_format(format); if (!text) return XLOG_RESULT_ERROR;
    va_list args; va_start(args,format);
    xlogresult result=xrtLogSourcePrintfV(logger,record->Level,record->Fields,record->FieldCount,
        record->File,record->Function,record->Line,record->ThreadId,text,args);
    va_end(args); xrtFree(text); return result;
}
