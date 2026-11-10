#ifndef XLS_ANALYSIS_DISPATCH_H
#define XLS_ANALYSIS_DISPATCH_H
#include "analysis/check.h"
enum xls_dispatch_kind {XLS_DISPATCH_CLASS=1,XLS_DISPATCH_PROTOCOL=2};
typedef struct xls_dispatch_info {size_t classes,protocols;} xls_dispatch_info;
typedef struct xls_dispatch_relation {
 uint32_t declaration,implementation,ownerType,contractType;
 size_t slot;unsigned kind;
} xls_dispatch_relation;
/* Original verified Typed XLIR slot/witness rows, not a new member lookup.
 * IDs are local to this checked compilation (owner/contract are IR TypeIds).
 * Borrow/validate the check just as for its typed index. No allocation, scan,
 * AST loan, JSON or I/O. Known-invalid/error owners reject and clear outputs.
 * These rows alone do NOT establish complete source symbol families: generic
 * origins, overload/property groups and whole-workspace coverage are separate. */
int xlsSemanticCheckDispatchInfo(const xls_semantic_check*,xls_dispatch_info*);
/* Class implementation rows first, then protocol implementation rows. Slot
 * ordinals remain zero-based in their own original domain. */
int xlsSemanticCheckDispatchRelation(const xls_semantic_check*,size_t,xls_dispatch_relation*);
#endif
