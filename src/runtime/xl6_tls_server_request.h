#ifndef XL6_TLS_SERVER_REQUEST_H
#define XL6_TLS_SERVER_REQUEST_H
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#endif
#include "xl6_runtime_export.h"

/* Private producer ABI for the typed TLS selector. A native request is only
 * borrowed during Copy; the returned opaque packet owns every protocol view
 * and byte in one allocation. It holds no Session, callback or language edge.
 * No raw request/view constructor is exposed to source-language programs. */
typedef struct XL6_TlsServerRequestV1 XL6_TlsServerRequestV1;
XL6_TEXT_API XL6_TlsServerRequestV1* xl6TlsServerRequestCopyV1(xbytesview,xbytesview);
XL6_TEXT_API void xl6TlsServerRequestDestroyV1(void*);
/* Returned values own their bytes independently of the request packet. */
XL6_TEXT_API xvalue* xl6TlsServerRequestNameV1(const void*);
XL6_TEXT_API int64_t xl6TlsServerRequestProtocolCountV1(const void*);
XL6_TEXT_API xvalue* xl6TlsServerRequestProtocolAtV1(const void*,int64_t);
#endif
