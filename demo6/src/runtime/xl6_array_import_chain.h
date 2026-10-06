#ifndef XL6_ARRAY_IMPORT_CHAIN_H
#define XL6_ARRAY_IMPORT_CHAIN_H
#include <stddef.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_ARRAY_IMPORT_CHAIN_API __declspec(dllexport)
#else
#define XL6_ARRAY_IMPORT_CHAIN_API
#endif
/* Explicit wire-data -> native-Array transaction, never an ordinary cast or
 * an implicit element admission fallback. The compiler supplies the complete
 * ordered Array schema chain, outermost first. Each seed is an independently
 * owned empty admitted ARRAY with its canonical resident projection bound.
 * Every adjacent edge is checked against the full owned child policy. A Stack
 * child is NOT fabricated from wire Array data; stop the chain at its parent.
 * Valid pointer/count (1..65) consumes and clears EVERY seed on BOTH paths,
 * including a NULL failed factory, malformed schema, BUSY or aliased seed.
 * Invalid pointer/count remains the caller's property. A prior factory error
 * propagates through NULL seeds. The borrowed source is retained for the
 * transaction and never modified. No seed may alias source or another seed.
 * Only wire Array children are recursively imported. Admitted native children
 * keep their actual identity/reference; a mismatched native child is refused.
 * Duplicate wire children at the same schema depth share one converted native
 * owner. Different depths are different domains. Memo keys retain source
 * shells, avoiding pointer reuse during allocator/destructor callbacks.
 * All templates are pinned until commit/rollback; no generated callbacks,
 * descriptors, contexts or element borrows survive this synchronous call.
 * Success returns the original outer seed, failure drops every partial owner
 * and preserves the primary error. Publication is single-result, not atomic
 * replacement of an already-published language container. */
XL6_ARRAY_IMPORT_CHAIN_API xvalue* xl6ArrayImportChainTakeV1(xvalue**, size_t, const xvalue*);
#endif
