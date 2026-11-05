#ifndef XL6_CAPTURE_V3_H
#define XL6_CAPTURE_V3_H
#include "xl6_capture_v2.h"
#include "xl6_native_capture_graph.h"

/* Same compact physical slot/layout as V2, with a separately admitted graph
 * vocabulary. V2 entry points continue to reject kind 11. Generic graph
 * storage has no dependency on any particular native view or UI package. */
XL6_CODE_LEASE_API const XL6_CaptureRuntimeV2* xl6CaptureRuntimeV3Get(void);
XL6_CODE_LEASE_API const XL6_ActivationCaptureRuntimeV2* xl6ActivationCaptureRuntimeV3Get(void);
#endif
