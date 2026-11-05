#ifndef XL6_CLOSURE_CAPTURE_V4_H
#define XL6_CLOSURE_CAPTURE_V4_H
#include "xl6_closure_capture_v3.h"
#include "xl6_capture_v3.h"
/* The existing compact layout now admits explicit native graph slots through
 * the V3 capture engine; the old V3 closure factory remains leaf-only. */
XL6_CODE_LEASE_API const XL6_ClosureRuntimeV3* xl6ClosureRuntimeV4Get(void);
#endif
