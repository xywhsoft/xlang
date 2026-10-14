#ifndef XL6_DICT_CALL_FRAME_H
#define XL6_DICT_CALL_FRAME_H
#include "xl6_call_frame_expansion.h"

/* Snapshot admitted native Dict<var> or real wire OBJECT keywords. Exact key
 * spans and owned values survive source mutation. Fixed slots remain borrowed.
 * State starts NULL; output may alias fixed and is unchanged on failure. */
XL6_TEXT_API bool xl6DictCallFrameExpandV1(uint64_t, uint64_t,
    const xvalue*, const xrtcallframe*, xrtcallframe*, void**);
#endif
