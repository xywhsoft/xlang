#ifndef XL6_CELL_NATIVE_H
#define XL6_CELL_NATIVE_H
#include "xl6_cell.h"
#include "xl6_capture_v2.h"
/* Empty owned leaf cell. All later transfers use the resident Cell V1 table,
 * never raw generated descriptors. Leaf values have no language graph edges. */
XL6_CODE_LEASE_API XL6_Cell* xl6CellCreateNativeV2(const XL6_NativeCaptureProfileV1*);
#endif
