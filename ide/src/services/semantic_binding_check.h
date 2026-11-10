#ifndef XIDE_SEMANTIC_BINDING_CHECK_H
#define XIDE_SEMANTIC_BINDING_CHECK_H
#include "services/rename_plan.h"
#ifdef XLS_COMPILER_XRT
enum xide_binding_check_result {
 XIDE_BINDING_PRESERVED,XIDE_BINDING_ARGUMENT,XIDE_BINDING_SOURCE_CHANGED,
 XIDE_BINDING_CHANGED,XIDE_BINDING_UNMAPPABLE,XIDE_BINDING_LIMIT,
 XIDE_BINDING_MEMORY,XIDE_BINDING_CANCELLED
};
typedef struct xide_binding_check_info {
 size_t sources,sourceBytesCompared,beforeReferences,afterReferences,
        mappedReferences,comparedReferences,bytes;
} xide_binding_check_info;
/* Compare ALL captured source-bound reference rows from two successful native
 * checks of the SAME compilation context. Prove the complete post-BOM source
 * vector: plan.before -> plan.after for edited sources, exact unchanged bytes
 * otherwise. Plans may also edit files belonging to other workspace owners.
 * Join lexical paths once, remap every original occurrence AND declaration
 * endpoint through the plan, and compare the complete bound-row multiset.
 * Numeric symbol/source IDs, spelling and a successful post-check alone are
 * never evidence of preserved binding. New/missing/changed rows all fail.
 * BOM coordinate prefixes in the second shard may differ (disk -> overlay);
 * native byte offsets always exclude BOM. Original plan source/prefix/kind
 * must agree. No I/O, parsing, checking, AST/cache retention or source writes.
 * Default32MiB/max64MiB charges scratch and held native path keys; borrowed
 * shards/plan, path conversion and allocator/sort internals are additional.
 * Cancellation polls rows/paths and64KiB text chunks; sorting is bounded but
 * not preemptible. Optional info is progress/charge, never an approval token.
 * Caller must supply complete same-context checks and prove source/config
 * freshness, workspace coverage, generic/foreign policy and writability.
 * Unbound/generated facts absent from the native shard are outside this
 * comparison. This helper alone does not authorize a rename or advertise LSP.
 */
enum xide_binding_check_result xideSemanticBindingsPreserved(
 const xide_reference_shard* before,const xide_reference_shard* after,
 const xide_rename_plan*,size_t,int (*cancelled)(void*),void*,xide_binding_check_info*);
#endif
#endif
