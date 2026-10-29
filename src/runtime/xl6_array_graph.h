#ifndef XL6_ARRAY_GRAPH_H
#define XL6_ARRAY_GRAPH_H
#include "xl6_text_value.h"
#include "xl6_value_graph_plan.h"

/* One XRT identity memo traverses wire containers and native Array slots.
 * DeepClone preserves the complete native layout/admission/projection;
 * DataSnapshot produces wire arrays throughout the graph, including mixed
 * objects. Opaque non-Array handles keep their downstream serialization
 * policy and are NOT cloned by DataSnapshot. Both borrow their source and
 * publish an owned result, or fail without changing the source. */
XL6_TEXT_API xvalue* xl6ValueDeepCloneV1(const xvalue*);
XL6_TEXT_API xvalue* xl6ValueDataSnapshotV1(const xvalue*);
/* Synchronous object projection shares the native Array adapter's one memo.
 * The callback/context are borrowed for this call only, never registered or
 * retained. Field selection must happen inside the callback, before Next. */
XL6_TEXT_API xvalue* xl6ValueDataProjectV1(const xvalue*, xvaluegraphobjectcopyv1, ptr,
    const xvaluegraphsequencev1*, size_t);
#endif
