#ifndef XL6_TLS_TICKET_RESULT_H
#define XL6_TLS_TICKET_RESULT_H
#include "xl6_tls_internal.h"
#include "xl6_native_class_result.h"
/* A real native result owner. No language graph edges; Resume is an immutable
 * native leaf, independently owned. Fields are filled only before publication. */
typedef struct XL6_TlsTicketResultV1 {xtlsresult status;xtlsresume* resume;} XL6_TlsTicketResultV1;
/* Fully constructs the actual class AND native handle before any producer or
 * queue pop. The independent class owner keeps *result valid on success. */
xvalue* xl6TlsTicketResultPrepareV1(XL6_NativeClassResultContextV1,XL6_CodeLease*,bool,XL6_TlsTicketResultV1**);
bool xl6TlsTicketResultSetV1(XL6_TlsTicketResultV1*,xtlsresult,xtlsresume**);
void xl6TlsTicketResultDestroyV1(void*);
int64_t xl6TlsTicketResultStatusV1(const void*);
void* xl6TlsTicketResultResumeV1(const void*);
#endif
