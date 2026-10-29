#include "xl6_net_frame_internal.h"
#include "xl6_net_buffer_abi.h"
#define FRAME_NAME xl6NetLineFramer
#define FRAME_INPUT(Raw) (&((XL6_NetLineFramerV1*)(Raw))->input)
#define FRAME_FEED_BUFFER
#include "xl6_net_frame_feed.inc"
#define FRAME_TYPE XL6_NetLineFramerV1
#define FRAME_PARSE xl6NetLineParseV1
#define FRAME_NEXT_INTO xl6NetLineFramerNextIntoV1
#include "xl6_net_frame_into.inc"
