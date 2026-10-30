#ifndef XLANG_ANALYSIS_INPUT_TOPOLOGY_H
#define XLANG_ANALYSIS_INPUT_TOPOLOGY_H
#include <stddef.h>
#include <stdint.h>
struct XL6_SourceOverlay;
typedef struct xls_input_topology xls_input_topology;
typedef struct xls_input_topology_scope {
    struct xls_input_topology_scope* previous;
    const struct XL6_SourceOverlay* overlay;
    void* state;
    int bound,failed;
} xls_input_topology_scope;
/* Private optional observation of EVERY original overlay lookup,including
 * misses. No resolver/search policy is added. Binding starts after overlay
 * construction. Allocation/limit/matcher failures disable only this proof.
 * Defaults8MiB completed keys and8MiB temporary capture keys,65536 paths;
 * container/allocator/transient conversion overhead is separate from RSS.
 * An immutable result owns its topology;all views borrow that result. */
void xlsInputTopologyBegin(xls_input_topology_scope* scope,size_t bytes);
void xlsInputTopologyBind(const struct XL6_SourceOverlay* overlay,size_t count);
void xlsInputTopologyLookup(const struct XL6_SourceOverlay* overlay,const char* absolute);
void xlsInputTopologyFailure(const struct XL6_SourceOverlay* overlay);
struct XL6_SourceReport;
void xlsInputTopologyReportFailure(const struct XL6_SourceReport* report);
xls_input_topology* xlsInputTopologyEnd(xls_input_topology_scope* scope);
void xlsInputTopologyDestroy(xls_input_topology* topology);
/* Explicit independent copy of immutable prepared lookup keys;no earlier
 * owner/report/overlay/AST is retained. Known payload<=min(limit,8MiB).
 * Cancellation/resource failure publishes no partial proof. Caller keeps the
 * source alive for the call;the returned proof owns all prepared key bytes. */
xls_input_topology* xlsInputTopologyClone(const xls_input_topology*,size_t limit,
    int (*cancelled)(void*),void* user);
typedef struct xls_input_topology_info {size_t paths,bytes;} xls_input_topology_info;
xls_input_topology_info xlsInputTopologyInfo(const xls_input_topology* topology);
/* Complete negative proof for changed overlay labels,not disk freshness.
 * Uses original xrtPathAbs and platform ordinal matching:Unicode-insensitive
 * Windows keys,exact UTF-8 elsewhere. Current overlays must also be unique
 * under that matcher,just as the original semantic constructors require.
 * No filesystem reads;key preparation may allocate. Failure declines reuse.
 * Does not mutate topology;safe simultaneous readers with a retained owner. */
int xlsInputTopologyAdmits(const xls_input_topology* topology,
    const char* const* changed,size_t changedCount,const char* const* current,size_t count);
struct xls_semantic_check;
struct xls_semantic_completion;
struct xls_input_batch;
const xls_input_topology* xlsSemanticCheckTopology(const struct xls_semantic_check* result);
const xls_input_topology* xlsSemanticCompletionTopology(const struct xls_semantic_completion* result);
/* Membership-only eligibility for original per-function semantic/IR memos;
 * actual changed function/global/type/source proofs still run independently.
 * Useful after exact topology hits have rebound the session batch while its
 * immutable model still owns the earlier unobserved overlay membership. */
int xlsInputTopologyOverlayCompatible(const xls_input_topology* topology,
    const struct XL6_SourceOverlay* previous,const struct XL6_SourceOverlay* current);
/* Extends the old strict-membership usage proof only when all added/removed
 * or reidentified labels were never consulted. Required entry/query and every
 * observed overlay retain exact identity/path/body;revision and disk guards
 * remain the caller's responsibility. Remaps usage into the current batch. */
int xlsInputBatchEqualTopology(const struct xls_input_batch* previous,const struct xls_input_batch* current,
    uint64_t used,const char* query,const xls_input_topology* topology,uint64_t* remapped);
#endif
