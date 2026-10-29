#ifndef XL6_NET_DATAGRAM_BATCH_INTERNAL_H
#define XL6_NET_DATAGRAM_BATCH_INTERNAL_H
#include "xl6_net_datagram_internal.h"
#include "xl6_buffer_internal.h"
#include "xl6_native_handle.h"
#include "xl6_net_batch_progress_internal.h"
typedef struct XL6_NetDatagramPacketV1 {
    /* One immutable owning edge to the actual Buffer native handle. No
     * generated wrapper, schema, callback, pointer view or code lease retained. */
    xvalue* buffer;
    xnetaddr target;
    XL6_NetDatagramInfoV1 info;
    int64_t status;
    bool hasTarget,hasResult;
} XL6_NetDatagramPacketV1;
static inline xbuffer* xl6NetDatagramPacketBufferV1(const XL6_NetDatagramPacketV1* packet)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!packet||!packet->buffer||!xrtValueGetHandle(packet->buffer,&raw,&ops,&user)||!raw||user||
        ops!=xl6BufferHandlePolicyV1Get()->Ops){xrtSetErrorKind(XERR_TYPE);return NULL;}
    return xl6BufferChecked(raw);
}
#endif
