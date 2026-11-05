#ifndef XL6_TREE_PROJECTION_H
#define XL6_TREE_PROJECTION_H
#include "xl6_tree.h"

/* Source storage is borrowed only for this synchronous call. The caller
 * proves its live typed-tree owner and key/value descriptor contract. The
 * independent, fresh ARRAY schema is consumed on EVERY path, including a
 * NULL source. It must be a sealed resident policy, not a rebound wire Array.
 * Slots are copied in key order into that policy's actual typed storage;
 * no per-item Value boxing, inferred layout or generated callbacks exist.
 * Failure publishes no partial owner and preserves the first error outcome. */
XL6_TEXT_API xvalue* xl6TypedTreeProjectTakeV2(xtypedtree*, bool, xvalue*,
    uint64_t, uint64_t, unsigned int, size_t, const xrttype*, const xvalueconverter*,const XL6_TreeKeyCodecV1*);
#endif
