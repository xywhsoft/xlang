#ifndef XL6_TLS_TRANSPORT_ABI_H
#define XL6_TLS_TRANSPORT_ABI_H
#include "xl6_runtime_export.h"
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif
#include "xl6_runtime_services.h"
#include "xl6_execution_language.h"
XL6_TEXT_API void* xl6TlsStreamCreateDefaultV1(XL6_ExecutionServicesV1,void*,const void*,void*,xstrview,int64_t);
XL6_TEXT_API void* xl6TlsStreamCreateV1(XL6_ExecutionServicesV1,void*,const void*,void*,xstrview,int64_t,xstrview,int64_t,const void*,const void*,int64_t);
XL6_TEXT_API void xl6TlsStreamDestroyV1(void*);
XL6_TEXT_API void xl6TlsStreamCloseV1(void*);
XL6_TEXT_API void xl6TlsStreamAbortV1(void*);
XL6_TEXT_API int64_t xl6TlsStreamStateV1(void*);
XL6_TEXT_API uint64_t xl6TlsStreamAvailableV1(void*);
XL6_TEXT_API uint64_t xl6TlsStreamPendingV1(void*);
XL6_TEXT_API void* xl6TlsStreamLocalV1(void*);
XL6_TEXT_API void* xl6TlsStreamRemoteV1(void*);
XL6_TEXT_API void* xl6TlsListenerCreateDefaultV1(XL6_ExecutionServicesV1,void*,const void*,void*);
XL6_TEXT_API void* xl6TlsListenerCreateV1(XL6_ExecutionServicesV1,void*,const void*,void*,const void*,int64_t,int64_t);
XL6_TEXT_API void xl6TlsListenerDestroyV1(void*);
XL6_TEXT_API void xl6TlsListenerCloseV1(void*);
XL6_TEXT_API int64_t xl6TlsListenerStateV1(void*);
XL6_TEXT_API void* xl6TlsListenerLocalV1(void*);
#endif
