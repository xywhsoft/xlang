#ifndef XL6_SET_SCHEMA_H
#define XL6_SET_SCHEMA_H
#include "xl6_set.h"

/* Optional resident nested-Set admission. Consumes a genuine child on both
 * paths and stores only full immutable layout/policy metadata in the shared
 * reference envelope. No child storage, wrapper, module span or source codec
 * is retained. Exact structural equivalence, not IDs/slot layout alone.
 * Mixed-family depth is limited to 64; only nullable defaults are absent.
 * This never authorizes a legacy wire Set or its language factory. */
XL6_TEXT_API xvalue* xl6SequenceSetCreateTakeV1(unsigned int,uint64_t,uint64_t,xvalue*,bool);
#endif
