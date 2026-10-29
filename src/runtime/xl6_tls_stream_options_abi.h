#ifndef XL6_TLS_STREAM_OPTIONS_ABI_H
#define XL6_TLS_STREAM_OPTIONS_ABI_H
#include "xl6_runtime_export.h"
#include <stdint.h>
XL6_TEXT_API void* xl6TlsStreamOptionsCreateV1(void);
XL6_TEXT_API void xl6TlsStreamOptionsDestroyV1(void*);
XL6_TEXT_API void* xl6TlsStreamOptionsWithTimeoutsV1(void*,int64_t,int64_t);
XL6_TEXT_API void* xl6TlsStreamOptionsWithAsyncLimitsV1(void*,uint64_t,int64_t,int64_t);
/* The private SDK deliberately ships declarations only (*.h). Keep this
 * boundary sibling-closed; implementation field tables stay out of it. */
XL6_TEXT_API int64_t xl6TlsStreamOptionsHandshakeTimeoutV1(void*);
XL6_TEXT_API int64_t xl6TlsStreamOptionsCloseTimeoutV1(void*);
XL6_TEXT_API uint64_t xl6TlsStreamOptionsAsyncBytesLimitV1(void*);
XL6_TEXT_API int64_t xl6TlsStreamOptionsAsyncCountLimitV1(void*);
XL6_TEXT_API int64_t xl6TlsStreamOptionsAsyncBatchV1(void*);
#endif
