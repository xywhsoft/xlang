#ifndef XL6_CALL_FRAME_EXPANSION_H
#define XL6_CALL_FRAME_EXPANSION_H
#include "xl6_text_value.h"

/* Detach before retiring every owned frame slot. Repeated finish is safe.
 * Preserve the first failure, including a checked failure without a diagnostic.
 * All frame views become invalid on finish. */
XL6_TEXT_API bool xl6CallFrameExpansionFinishV1(void**, bool);
#endif
