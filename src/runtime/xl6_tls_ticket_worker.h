#ifndef XL6_TLS_TICKET_WORKER_H
#define XL6_TLS_TICKET_WORKER_H
#include "xl6_tls_data_internal.h"
#include "xl6_task_frame_internal.h"
#include "xl6_task_net.h"
/* Permanent resident operation, never a generated/source callback. The actual
 * creator-specific provider selects optional client/server resume code. */
typedef struct XL6_TlsTicketCommandV1 {
    xtlsrole role;
    bool mutates;
    XL6_FuturePayloadKind payload;
    XL6_TaskOutcome (*Run)(xtlsstream*,XL6_NativeClassResultContextV1,XL6_CodeLease*,
        xcancel*,xbytesview,uint32_t,XL6_TaskResult*);
} XL6_TlsTicketCommandV1;
struct xfuture* xl6TlsTicketSubmitV1(const XL6_NativeClassResultContextV1*,void*,
    const XL6_TlsTicketCommandV1*,xbytesview,uint32_t);
void xl6TlsTicketClassSyncV1(XL6_NativeClassResultContextV1,void*,
    const XL6_TlsTicketCommandV1*,xbytesview,uint32_t,struct xvalue**);
uint64_t xl6TlsTicketScalarSyncV1(void*,const XL6_TlsTicketCommandV1*,int64_t);
const XL6_TlsTicketCommandV1* xl6TlsTicketTakeCommandV1Get(void);
const XL6_TlsTicketCommandV1* xl6TlsTicketCountCommandV1Get(void);
const XL6_TlsTicketCommandV1* xl6TlsTicketDroppedCommandV1Get(void);
const XL6_TlsTicketCommandV1* xl6TlsTicketIssueCommandV1Get(void);
const XL6_TlsTicketCommandV1* xl6TlsTicketNewCommandV1Get(void);
bool xl6TlsTicketServerInputV1(const struct xvalue*,int64_t,int64_t,xbytesview*);
#endif
