#ifndef XL6_ARRAY_CALL_FRAME_H
#define XL6_ARRAY_CALL_FRAME_H
#include "src/runtime/xl6_text_value.h"

/* Compiler-private producer contract: an independent native ARRAY of var
 * references, with exact container/element identities and resident projection.
 * Only the positional tail is packed; fixed/default/keyword binding belongs
 * to the caller's verified bridge plan. The borrowed frame is never consumed
 * or retained. Failure publishes no partial Array and preserves the first
 * error through cleanup. No source descriptor or callback is captured. */
XL6_TEXT_API xvalue* xl6ArrayCallFramePackV1(uint64_t, uint64_t, const xrtcallframe*, size_t);
#endif
