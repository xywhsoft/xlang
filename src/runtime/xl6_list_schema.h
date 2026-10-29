#ifndef XL6_LIST_SCHEMA_H
#define XL6_LIST_SCHEMA_H
#include "xl6_list.h"

/* Optional resident nested-List admission component. Consumes a genuine child
 * on both paths, captures only its full immutable List layout/policy, and
 * constructs an ARRAY/STACK reference domain using the shared schema envelope.
 * A List parent consumes that empty ARRAY domain through ListCreateTakeV1;
 * no child Value/storage or generated callback is retained by the schema.
 * Independently constructed equivalent policies compare structurally; IDs or
 * physical layout alone never authorize a wider logical child domain.
 * Depth includes all resident container families and is bounded at 64.
 * Nonnullable elements have the child's identity; nullable elements differ.
 * Nullable defaults are absent, nonnullable growth has no invented child.
 * Does not register the source List carrier or change persistence formats. */
XL6_TEXT_API xvalue* xl6SequenceListCreateTakeV1(unsigned int,uint64_t,uint64_t,xvalue*,bool);
#endif
