#ifndef XL6_WS_STREAM_INTERNAL_H
#define XL6_WS_STREAM_INTERNAL_H
#include "xl6_ws_upgrade_internal.h"
#include "xl6_execution_resource_internal.h"
#include "xl6_native_class_callback.h"
#include "xl6_net_buffer_internal.h"

typedef struct XL6_WsEvent {
    XL6_ExecutionResourceV1* stream;
    xwsmessageinfo message;
    xwsstreamclose close;
    xbytesview data;
    xstrview errorMessage,errorDomain;
    size_t pending;
    int64_t errorCode;
    int kind,status;
    char storage[];
} XL6_WsEvent;
XL6_WsEvent* xl6WsEventCopyV2(int,const xwsmessageinfo*,xbytesview,size_t,const xwsstreamclose*,const xerror*);

typedef struct XL6_WsResource {
    XL6_ExecutionResourceV1 base;
    XL6_ExecutionResourceV1* parent;
    XL6_Closure* callback;
    xvalue* outputOwner;
    xbuffer* output; /* protected by outputOwner, only used on the worker */
    xatomicptr native,error;
    xatomic32 retired,graceful,failed,prepared;
    uint64_t worker;
    xwsmessageinfo message;
    size_t delivered;
    bool outputDataEvents;
    const struct XL6_WsWaitPolicy* waitPolicy;
    struct XL6_WsWaiter* waits;
    size_t waitCount;
    uint64_t waitTimer,waitDue;
    xatomic32 sweepQueued;
} XL6_WsResource;
/* Optional permanent resident wait provider. Typed physical slots keep this
 * feature out of ordinary event-only streams. No module descriptor is stored. */
typedef struct XL6_WsWaitPolicy {
    bool (*Notify)(XL6_WsResource*,xwsstream*,int);
    bool (*Trace)(const XL6_WsResource*,xrtownershipvisitor,void*);
    bool (*Drop)(XL6_WsResource*);
} XL6_WsWaitPolicy;
typedef struct XL6_WsCreateOptions {
    void* transport;
    XL6_NetBufferV1* initial;
    const XL6_WsUpgrade* upgrade;
    const xwsstreamconfig* config;
    XL6_Closure* callback;
    /* Call-only permanent resident builder, never an external/module callback. */
    xwsstream* (*Build)(XL6_WsResource*,const struct XL6_WsCreateOptions*);
} XL6_WsCreateOptions;
const XL6_ExecutionFamilyV1* xl6WsExecutionFamilyV2Get(void);
const xwsstreamownershipv1* xl6WsStreamDataPolicyV2Get(void);
bool xl6WsStreamBindInternalV2(void*,void*,void*,void*,void*,
    xwsstream* (*)(XL6_WsResource*,const XL6_WsCreateOptions*));
bool xl6WsStreamEnterV2(void*,bool,XL6_ExecutionScopeV1*,xwsstream**);
void xl6WsStreamFailV2(void);
void xl6WsStreamRecordFailureV2(XL6_WsResource*,xerror*);
#endif
