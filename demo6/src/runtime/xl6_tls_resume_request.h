#ifndef XL6_TLS_RESUME_REQUEST_H
#define XL6_TLS_RESUME_REQUEST_H
#include "xl6_tls_server_request.h"
typedef struct XL6_TlsResumeRequestV1 XL6_TlsResumeRequestV1;
/* Call-only native borrow. The returned leaf owns every byte independently. */
XL6_TEXT_API XL6_TlsResumeRequestV1* xl6TlsResumeRequestCopyV1(const xtlsserverresumerequest*);
XL6_TEXT_API void xl6TlsResumeRequestDestroyV1(void*);
XL6_TEXT_API xvalue* xl6TlsResumeRequestNameV1(const void*);
XL6_TEXT_API int64_t xl6TlsResumeRequestProtocolCountV1(const void*);
XL6_TEXT_API xvalue* xl6TlsResumeRequestProtocolAtV1(const void*,int64_t);
XL6_TEXT_API xvalue* xl6TlsResumeRequestTicketV1(const void*);
XL6_TEXT_API uint64_t xl6TlsResumeRequestAgeV1(const void*);
#endif
