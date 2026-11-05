#ifndef XL6_TLS_DIAL_OPTIONS_ABI_H
#define XL6_TLS_DIAL_OPTIONS_ABI_H
#include "xl6_runtime_export.h"
#include <stdint.h>
#include <stdbool.h>
typedef struct xstrview xstrview;
XL6_TEXT_API void* xl6TlsDialOptionsCreateV1(void);
XL6_TEXT_API void xl6TlsDialOptionsDestroyV1(void*);
XL6_TEXT_API void* xl6TlsDialOptionsTransportV1(const void*);
XL6_TEXT_API void* xl6TlsDialOptionsStreamV1(const void*);
XL6_TEXT_API void* xl6TlsDialOptionsWithTransportV1(const void*,const void*);
XL6_TEXT_API void* xl6TlsDialOptionsWithStreamV1(const void*,const void*);
XL6_TEXT_API int64_t xl6TlsDialOptionsTimeoutV1(const void*);
XL6_TEXT_API void* xl6TlsDialOptionsWithTimeoutV1(const void*,int64_t);
XL6_TEXT_API bool xl6TlsDialOptionsServerNameFromHostV1(const void*);
XL6_TEXT_API void* xl6TlsDialOptionsWithServerNameFromHostV1(const void*,bool);
XL6_TEXT_API xstrview xl6TlsDialOptionsServerNameV1(const void*);
XL6_TEXT_API xstrview xl6TlsDialOptionsVerifyNameV1(const void*);
XL6_TEXT_API void* xl6TlsDialOptionsWithServerNameV1(const void*,xstrview,int64_t);
XL6_TEXT_API void* xl6TlsDialOptionsWithVerifyNameV1(const void*,xstrview,int64_t);
#endif
