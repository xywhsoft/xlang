#ifndef XL6_NET_FRAME_INTERNAL_H
#define XL6_NET_FRAME_INTERNAL_H
#include "xl6_net_buffer_internal.h"
typedef struct XL6_NetFrameResultV1 {
    xnetframe frame;
    uint64_t issuer,revision;
    bool ready;
    uint8_t data[];
} XL6_NetFrameResultV1;
typedef struct XL6_NetLineFramerV1 {XL6_NetBufferV1 input;xnetlineframer parser;uint64_t issuer,revision;uint8_t delimiter[];} XL6_NetLineFramerV1;
typedef struct XL6_NetLengthFramerV1 {XL6_NetBufferV1 input;xnetlengthframer parser;uint64_t issuer,revision;} XL6_NetLengthFramerV1;
xnetframestatus xl6NetLineParseV1(XL6_NetLineFramerV1*,xnetframe*);
xnetframestatus xl6NetLengthParseV1(XL6_NetLengthFramerV1*,xnetframe*);
/* A successful snapshot owns copied payload bytes only, never the input chain,
 * borrowed cursors, module code or a caller Buffer. MORE is immutable data. */
void* xl6NetFrameMoreV1(void);
void* xl6NetFrameCopyV1(const XL6_NetBufferV1*,const xnetframe*,uint64_t,uint64_t);
const XL6_NetFrameResultV1* xl6NetFrameReadyV1(const void*);
bool xl6NetFrameIdentityV1(uint64_t*);
static inline bool xl6NetFrameCanAdvanceV1(uint64_t revision)
{if(revision==UINT64_MAX){xrtSetErrorKind(XERR_RANGE);return false;}return true;}
#endif
