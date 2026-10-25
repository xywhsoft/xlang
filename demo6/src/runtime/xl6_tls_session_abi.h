#ifndef XL6_TLS_SESSION_ABI_H
#define XL6_TLS_SESSION_ABI_H
#include "xl6_runtime_export.h"
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif
#include "xl6_native_class_abi.h"
#include "xl6_product_abi.h"
XL6_TEXT_API void* xl6TlsSessionClientCreateV1(void*,xstrview,int64_t,xstrview,int64_t);
XL6_TEXT_API void* xl6TlsSessionServerCreateV1(void*);
XL6_TEXT_API void xl6TlsSessionDestroyV1(void*);
XL6_TEXT_API void xl6TlsSessionDisposeV1(void*);
XL6_TEXT_API bool xl6TlsSessionDisposedV1(void*);
XL6_TEXT_API bool xl6TlsSessionResumedV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionRoleV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionStateV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionVersionV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionCipherV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionWaitV1(void*);
XL6_TEXT_API bool xl6TlsSessionReadyV1(void*);
XL6_TEXT_API uint64_t xl6TlsSessionPendingInputV1(void*);
XL6_TEXT_API uint64_t xl6TlsSessionPendingCipherV1(void*);
XL6_TEXT_API uint64_t xl6TlsSessionPendingPlainV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionDriveV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionCloseV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionEofV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionFeedTextV1(void*,xstrview,int64_t);
XL6_TEXT_API int64_t xl6TlsSessionFeedBytesV1(void*,const xvalue*,int64_t);
XL6_TEXT_API int64_t xl6TlsSessionFeedBufferV1(void*,void*);
XL6_TEXT_API XL6_ProductI64U64V1 xl6TlsSessionWriteTextV1(void*,xstrview,int64_t);
XL6_TEXT_API XL6_ProductI64U64V1 xl6TlsSessionWriteBytesV1(void*,const xvalue*,int64_t);
XL6_TEXT_API XL6_ProductI64BytesV1 xl6TlsSessionReadV1(void*,int64_t);
XL6_TEXT_API xvalue* xl6TlsSessionTakeCipherV1(void*,int64_t);
XL6_TEXT_API xvalue* xl6TlsSessionProtocolV1(void*);
XL6_TEXT_API int64_t xl6TlsSessionPeerCertificateCountV1(void*);
XL6_TEXT_API void* xl6TlsSessionPeerCertificateAtV1(void*,int64_t);
XL6_TEXT_API int64_t xl6TlsSessionKeyUpdateV1(void*,bool);
XL6_TEXT_API XL6_ProductBoolI64I64V1 xl6TlsSessionPeerAlertV1(void*);
XL6_TEXT_API xvalue* xl6TlsSessionServerNameV1(void*);
XL6_TEXT_API uint64_t xl6TlsSessionServerCookieV1(void*);
#endif
