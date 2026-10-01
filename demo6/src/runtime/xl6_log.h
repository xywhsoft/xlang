#ifndef XL6_LOG_H
#define XL6_LOG_H
#include <stdbool.h>
#include <stdint.h>
#if !defined(XRT_SINGLE_HEADER)
#include "xruntime_decl.h"
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
#endif
