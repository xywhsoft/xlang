#ifndef XL6_ARRAY_CALL_FRAME_H
#define XL6_ARRAY_CALL_FRAME_H
#include "xl6_call_frame_expansion.h"

/* Compiler-private producer contract: an independent native ARRAY of var
 * references, with exact container/element identities and resident projection.
 * Only the positional tail is packed; fixed/default/keyword binding belongs
 * to the caller's verified bridge plan. The borrowed frame is never consumed
 * or retained. Failure publishes no partial Array and preserves the first
 * error through cleanup. No source descriptor or callback is captured. */
XL6_TEXT_API xvalue* xl6ArrayCallFramePackV1(uint64_t, uint64_t, const xrtcallframe*, size_t);
/* Independently owned positional frame slots from an admitted native ARRAY
 * of var. Fixed inputs remain borrowed. State must start NULL;
 * output may alias fixed and remains untouched on failure. The frame and its
 * owner are published together on success. No view survives Finish.
 * Finish detaches before dropping, closes all owners and preserves the first
 * failure, including a diagnostic-free checked failure. */
XL6_TEXT_API bool xl6ArrayCallFrameExpandV1(uint64_t, uint64_t,
    const xvalue*, const xrtcallframe*, xrtcallframe*, void**);
#endif
