#ifndef XL6_CELL_NATIVE_GRAPH_H
#define XL6_CELL_NATIVE_GRAPH_H
#include "xl6_cell.h"
#include "xl6_native_capture_graph.h"
/* Empty graph-owning cell; transfers use the ordinary opaque Cell table.
 * Profiles are permanent resident metadata. No leaf admission or boxing. */
XL6_CODE_LEASE_API XL6_Cell* xl6CellCreateNativeGraphV3(const XL6_NativeCaptureProfileV1*);
#endif
