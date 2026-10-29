#include "xl6_net_frame_internal.h"
#include "xl6_net_buffer_abi.h"
#define FRAME_NAME xl6NetLineFramer
#define FRAME_INPUT(Raw) (&((XL6_NetLineFramerV1*)(Raw))->input)
#define FRAME_FEED_VALUE
#include "xl6_net_frame_feed.inc"
