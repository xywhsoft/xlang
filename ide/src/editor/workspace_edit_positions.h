#ifndef XIDE_WORKSPACE_EDIT_POSITIONS_H
#define XIDE_WORKSPACE_EDIT_POSITIONS_H
#include "protocol/workspace_edit.h"
typedef struct xide_edit_positions xide_edit_positions;
enum xide_edit_affinity {XIDE_EDIT_LEFT,XIDE_EDIT_RIGHT};
typedef struct xide_edit_selection {size_t anchor,caret;} xide_edit_selection;
/* Worker preparation; retain the immutable plan, trim unchanged scalar-aligned
 * edges of each replacement with the shared text-diff core. Max65536 edits;
 * known quota includes plan.Info.bytes + mapping storage (default128/max256MiB),
 * not allocator/RSS. Cancellation does not escape this call. No GUI links.
 * Destroy may release large last-owner plan snapshots; retire off the UI.
 * No diff inside a changed middle, semantic/grapheme tracking or content apply. */
xide_edit_positions* xideEditPositionsPrepare(xide_workspace_edit*,size_t budget,
 int (*cancelled)(void*),void*,enum xide_workspace_edit_error*);
void xideEditPositionsDestroy(xide_edit_positions*);
/* Known retained plan + map allocations, for a combined preparation quota. */
size_t xideEditPositionsStorage(const xide_edit_positions*);
/* Read concurrently under externally held lifetime. Constant scalar-boundary
 * checks and O(log edits) lookups, no allocation/whole-line/body scan.
 * Offsets are original UTF8 bytes. Reject out-of-range/continuation interiors
 * without changing outputs. Unchanged prefix/suffix/inter-edit content keeps
 * its position. Inside a changed middle, LEFT/RIGHT maps to its leading/trailing
 * edge; at equal-point inserts LEFT precedes all and RIGHT follows all.
 * Whole unchanged document maps identically, regardless of no-op wire edits. */
int xideEditPositionMap(const xide_edit_positions*,size_t document,size_t offset,
 enum xide_edit_affinity,size_t* mapped);
/* Collapsed caret follows inserts (RIGHT for both). Nonempty selection expands
 * at edited borders: leading endpoint LEFT, trailing RIGHT. Preserve orientation;
 * deleted ranges may collapse. Other XUI selection flags remain caller-owned. */
int xideEditSelectionMap(const xide_edit_positions*,size_t document,
 xide_edit_selection original,xide_edit_selection* mapped);
#endif
