#ifndef XL6_FUTURE_CALLBACK_INTERNAL_H
#define XL6_FUTURE_CALLBACK_INTERNAL_H
#include "xl6_future_callback.h"
#include "xl6_continuation_queue.h"
/* Same-source resident ABI. Inline and queued transports share ONE physical
 * activation and capture lifetime. Queue/Watch slots are uniquely contained;
 * their real references pin this allocation, never a synthetic frame shell. */
typedef struct XL6_FutureCallbackFrameV1 {
    xatomic64 gate;
    XL6_CodeLease* lease;
    XL6_FutureCallbackProc proc;
    XL6_FutureCallbackDrop drop;
    XL6_CodeContextTrace trace;
    const XL6_CaptureLayoutV1* layout;
    const XL6_CaptureLayoutV2* layoutV2;
    size_t references, active, dataSize;
    const void* claim;
    bool committed, publishing, retiring, payloadLive, cleared, queued;
    xfuturewatch watch;
    xfuture* source;
    xpromise* output;
    xcancel* cancel;
    xcancelwatch* cancelSource;
    /* Optional aligned tail of this SAME allocation; native inline frames
     * do not pay for an unused ticket or a second heap allocation. */
    XL6_ContinuationTicket* ticket;
    max_align_t alignment;
    unsigned char data[];
} XL6_FutureCallbackFrameV1;
extern const xrtownershipops xl6FutureCallbackFrameOpsV1;
static inline XL6_FutureCallbackFrameV1* xl6CallbackFrameHeaderV1(const void* data)
{return data?(XL6_FutureCallbackFrameV1*)((unsigned char*)data - offsetof(XL6_FutureCallbackFrameV1,data)):NULL;}
void xl6CallbackFrameBeginV1(XL6_FutureCallbackFrameV1*,xrtownershipscope*);
void xl6CallbackFrameEndV1(XL6_FutureCallbackFrameV1*,xrtownershipscope*);
void xl6CallbackFrameReleaseV1(XL6_FutureCallbackFrameV1*);
bool xl6CallbackFrameBindOutputV1(XL6_FutureCallbackFrameV1*);
void xl6CallbackFrameDetachV1(XL6_FutureCallbackFrameV1*);
void xl6CallbackFrameInvokeV1(XL6_FutureCallbackFrameV1*);
/* Cleanup on the designated executor; consume only for legacy/private owners.
 * Queued release leaves the actual queue reference to Storage.Release. */
void xl6CallbackFrameRetireV1(XL6_FutureCallbackFrameV1*,bool consume);
struct xl6_foreign_error_result xl6CallbackFrameCleanupResultV1(XL6_FutureCallbackFrameV1*,struct xl6_foreign_error_result);
void* xl6CallbackFrameAllocateQueuedV1(const XL6_CodeOwner*,size_t,XL6_FutureCallbackProc,const XL6_CaptureLayoutV2*);
#endif
