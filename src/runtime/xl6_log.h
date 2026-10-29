#ifndef XL6_LOG_H
#define XL6_LOG_H
#include <stdbool.h>
#include <stdint.h>
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif

#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_LOG_API __declspec(dllexport)
#else
#define XL6_LOG_API
#endif

/* Resident adapter ABI. Strings are owned XRT allocations on return and
 * borrowed, exact-length views on input; no generated implementation lives
 * in this header. Handle results transfer exactly one native reference. */
XL6_LOG_API void* xl6LogCreateV1(xstrview name, int64_t level);
XL6_LOG_API void* xl6LogTakeV1(void* logger);
XL6_LOG_API void xl6LogDestroyV1(void* logger);
XL6_LOG_API void* xl6LogHandleV1(void* logger);
XL6_LOG_API xstrview xl6LogNameV1(void* logger);
XL6_LOG_API int64_t xl6LogLevelV1(void* logger);
XL6_LOG_API bool xl6LogSetLevelV1(void* logger, int64_t level);
XL6_LOG_API bool xl6LogEnabledV1(void* logger, int64_t level);
XL6_LOG_API bool xl6LogAttachV1(void* logger, void* sink);
XL6_LOG_API bool xl6LogDetachV1(void* logger, void* sink);
XL6_LOG_API int64_t xl6LogDetachAllV1(void* logger);
XL6_LOG_API int64_t xl6LogSinkCountV1(void* logger);
XL6_LOG_API int64_t xl6LogWriteV1(void* logger, int64_t level, xstrview message, int64_t size);
XL6_LOG_API bool xl6LogFlushV1(void* logger);
XL6_LOG_API void* xl6LogDefaultV1(void);
XL6_LOG_API bool xl6LogSetDefaultV1(void* logger);
XL6_LOG_API bool xl6LogReleaseDefaultV1(void* expected);
XL6_LOG_API int64_t xl6LogDefaultWriteV1(int64_t level, xstrview message, int64_t size);
XL6_LOG_API bool xl6LogDefaultFlushV1(void);
XL6_LOG_API xstrview xl6LogLevelNameV1(int64_t level);
XL6_LOG_API void* xl6LogSinkTakeV1(void* sink);
XL6_LOG_API void xl6LogSinkDestroyV1(void* sink);
XL6_LOG_API void* xl6LogSinkHandleV1(void* sink);
XL6_LOG_API xstrview xl6LogSinkNameV1(void* sink);
XL6_LOG_API int64_t xl6LogSinkLevelV1(void* sink);
XL6_LOG_API bool xl6LogSinkSetLevelV1(void* sink, int64_t level);
XL6_LOG_API bool xl6LogSinkFlushV1(void* sink);
XL6_LOG_API void* xl6LogStatsV1(void* owner, bool sink);
XL6_LOG_API void* xl6LogStatsTakeV1(void* snapshot);
XL6_LOG_API void xl6LogStatsDestroyV1(void* snapshot);
XL6_LOG_API uint64_t xl6LogStatsCounterV1(void* snapshot, int64_t counter);
XL6_LOG_API void* xl6LogConsoleV1(int64_t level, int64_t target, int64_t color,
    int64_t errorLevel, bool flush, int64_t style, int64_t flags, int64_t utcOffset);
/* Structured records are independent owned snapshots. */
XL6_LOG_API void* xl6LogRecordCreateV1(int64_t level, xstrview message, int64_t size);
XL6_LOG_API void* xl6LogRecordCloneV1(void* record);
XL6_LOG_API void xl6LogRecordDestroyV1(void* record);
XL6_LOG_API void* xl6LogRecordHandleV1(void* record);
XL6_LOG_API int64_t xl6LogRecordLevelV1(void* record);
XL6_LOG_API xstrview xl6LogRecordStringV1(void* record, int64_t index);
XL6_LOG_API int64_t xl6LogRecordTimeV1(void* record);
XL6_LOG_API uint64_t xl6LogRecordNumberV1(void* record, int64_t index);
XL6_LOG_API bool xl6LogRecordSourceV1(void* record, xstrview file, int64_t line, xstrview function);
XL6_LOG_API bool xl6LogRecordMetadataV1(void* record, xstrview logger, int64_t time, uint64_t thread);
XL6_LOG_API bool xl6LogRecordFieldV1(void* record, xstrview name, xvalue* value, int64_t size);
XL6_LOG_API bool xl6LogRecordErrorV1(void* record, xstrview name, xerror* error);
XL6_LOG_API xstrview xl6LogRecordFieldNameV1(void*, int64_t);
XL6_LOG_API int64_t xl6LogRecordFieldTypeV1(void*, int64_t);
XL6_LOG_API xvalue* xl6LogRecordFieldValueV1(void*, int64_t);
XL6_LOG_API xerror* xl6LogRecordFieldErrorV1(void*, int64_t);
XL6_LOG_API int64_t xl6LogWriteRecordV1(void* logger, void* record);
XL6_LOG_API int64_t xl6LogDefaultRecordV1(void* record);
XL6_LOG_API bool xl6LogTextConfig(xlogtextconfig*, int64_t, int64_t, int64_t);
XL6_LOG_API bool xl6LogJsonConfig(xlogjsonconfig*, int64_t, int64_t, int64_t, int64_t, int64_t);
XL6_LOG_API bool xl6LogTextValidateV1(int64_t, int64_t, int64_t);
XL6_LOG_API bool xl6LogJsonValidateV1(int64_t, int64_t, int64_t, int64_t, int64_t);
XL6_LOG_API xstrview xl6LogTextV1(void*, int64_t, int64_t, int64_t);
XL6_LOG_API xstrview xl6LogJsonV1(void*, int64_t, int64_t, int64_t, int64_t, int64_t);
XL6_LOG_API int64_t xl6LogTextBufferV1(void*, void*, int64_t, int64_t, int64_t);
XL6_LOG_API int64_t xl6LogJsonBufferV1(void*, void*, int64_t, int64_t, int64_t, int64_t, int64_t);
XL6_LOG_API bool xl6LogSinkAffinityV1(void*, bool, void*);
XL6_LOG_API void* xl6LogSinkAffinityContextV1(void*);
XL6_LOG_API bool xl6LogSinkWorkerSafeV1(void*);
XL6_LOG_API void* xl6LogCustomV1(xstrview name, int64_t level, xvalue* write, xvalue* flush);
XL6_LOG_API bool xl6LogCustomCloseV1(void*);
XL6_LOG_API void* xl6LogTextFileV1(xstrview, int64_t, int64_t, int64_t, uint64_t, int64_t, int64_t, int64_t, uint64_t, int64_t, int64_t, int64_t);
XL6_LOG_API void* xl6LogJsonFileV1(xstrview, int64_t, int64_t, int64_t, uint64_t, int64_t, int64_t, int64_t, uint64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
XL6_LOG_API xstrview xl6LogFilePathV1(void*);
XL6_LOG_API bool xl6LogFileRotateV1(void*);
XL6_LOG_API bool xl6LogFileReopenV1(void*);
XL6_LOG_API bool xl6LogFileCloseV1(void*);
XL6_LOG_API uint64_t xl6LogFileCounterV1(void*, int64_t);
XL6_LOG_API void* xl6LogFileStatsV1(void*);
XL6_LOG_API void* xl6LogFileStatsTakeV1(void*);
XL6_LOG_API void xl6LogFileStatsDestroyV1(void*);
XL6_LOG_API void* xl6LogAsyncV1(void*, xstrview, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
XL6_LOG_API void* xl6LogRingV1(void*, xstrview, int64_t, int64_t, int64_t, int64_t, int64_t, uint64_t);
XL6_LOG_API bool xl6LogAsyncStopV1(void*);
XL6_LOG_API xerror* xl6LogAsyncErrorV1(void*);
XL6_LOG_API void* xl6LogAsyncStatsV1(void*);
XL6_LOG_API bool xl6LogRingStopV1(void*);
XL6_LOG_API xerror* xl6LogRingErrorV1(void*);
XL6_LOG_API void* xl6LogRingStatsV1(void*);
XL6_LOG_API void* xl6LogQueueStatsTakeV1(void*);
XL6_LOG_API void xl6LogQueueStatsDestroyV1(void*);
XL6_LOG_API uint64_t xl6LogQueueCounterV1(void*, int64_t);
XL6_LOG_API int64_t xl6LogTextWriterV1(void*, void*, int64_t, int64_t, int64_t);
XL6_LOG_API int64_t xl6LogJsonWriterV1(void*, void*, int64_t, int64_t, int64_t, int64_t, int64_t);
XL6_LOG_API int64_t xl6LogPrintfV1(void*, int64_t, xstrview, ...);
XL6_LOG_API int64_t xl6LogDefaultPrintfV1(int64_t, xstrview, ...);
XL6_LOG_API int64_t xl6LogRecordPrintfV1(void*, void*, xstrview, ...);
#endif
