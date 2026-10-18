#ifndef XL6_CHANNEL_INTERNAL_H
#define XL6_CHANNEL_INTERNAL_H
#include "xl6_channel.h"
#include "xl6_channel_wait_abi.h"
/* Resident-only operation credit. Owner/schema and boxed send ownership stay
 * pinned through the actual native wait/unregistration/cleanup tail. Graph
 * observation refuses active owners; no source callback or hidden root is kept.
 * Open requires an all-zero record; Close consumes it on EVERY outcome.
 * Message/received are opaque boxes, never element pointers. */
typedef struct XL6_ChannelOperationV1 {
    void* owner;
    xchannel* native;
    void* message;
    void* received;
    XL6_ChannelSlotV1 target;
    bool sending;
} XL6_ChannelOperationV1;
XL6_TEXT_API bool xl6ChannelOperationOpenV1(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,bool,XL6_ChannelStatusV1*,XL6_ChannelOperationV1*);
XL6_TEXT_API bool xl6ChannelOperationCommitV1(XL6_ChannelOperationV1*);
XL6_TEXT_API void xl6ChannelOperationCloseV1(XL6_ChannelOperationV1*);
typedef struct XL6_ChannelSelectFrameV1 {
    size_t count;
    XL6_ChannelOperationV1* operations;
    xchannelcase* native;
    XL6_ChannelOperationV1 inlineOperations[8];
    xchannelcase inlineNative[8];
} XL6_ChannelSelectFrameV1;
XL6_TEXT_API bool xl6ChannelSelectPrepareV1(const XL6_ChannelCaseV1*,size_t,
    XL6_ChannelSelectionV1*,XL6_ChannelSelectFrameV1*);
/* Complete transfers the selected message BEFORE releasing any operation
 * credit, even if native cleanup has already signalled an ambient error. */
XL6_TEXT_API bool xl6ChannelSelectCompleteV1(XL6_ChannelSelectFrameV1*,xchannelselectresult,
    XL6_ChannelSelectionV1*);
XL6_TEXT_API void xl6ChannelSelectDiscardV1(XL6_ChannelSelectFrameV1*);
#endif
