#ifndef XLANG_ANALYSIS_COMPLETION_UNIT_H
#define XLANG_ANALYSIS_COMPLETION_UNIT_H
#include "analysis/semantic.h"
/* Actual construction work for a query-token view. A view owns independent
 * candidate/source units,never an earlier checked AST/result. Zero means the
 * original semantic route. Explicit AST exposure can run the original current
 * query on demand; it is not charged as typing-query work. Byte accounting is
 * known admitted payload,not allocator overhead/global RSS/a transient peak. */
typedef struct xls_completion_unit_work {
    uint64_t projected,proofNodes,candidates,knownBytes,astMaterialized,astFailures;
    uint64_t scopeComplete,scopeCandidates;
} xls_completion_unit_work;
xls_completion_unit_work xlsSemanticCompletionUnitWork(const xls_semantic_completion*);
/* Same-thread admission/failure verification. Nested controls only shrink
 * the256MiB known retained-data allowance;zero disables the optional route.
 * Existing syntax may independently materialize a read-only view afterwards,
 * and explicit full semantic AST demand has its own original budgets. These,
 * transient clone maps,allocator internals and RSS are outside this allowance. */
typedef struct xls_completion_unit_control {struct xls_completion_unit_control* previous;size_t bytes;} xls_completion_unit_control;
void xlsCompletionUnitBegin(xls_completion_unit_control*,size_t bytes);
void xlsCompletionUnitEnd(xls_completion_unit_control*);
struct xls_input_batch;struct xls_syntax;
/* Private session opt-in. All other inputs/identities/context must be exact;
 * original disk/probe/environment validation precedes and follows publication.
 * Current syntax is produced by the original parser. Every logical parser AST
 * value is checked directly on immutable chunks with exact coordinate mapping.
 * A complete original-emission pool admits widened/replaced prefixes too;
 * without that optional producer, only the original prefix subset is valid. Any
 * unknown/missing/cancelled/resource proof returns NULL for ordinary fallback.
 * Both syntax bodies are matched against their actual captured batches. The
 * producer,batches and syntax snapshots are pinned before the first callback;
 * a callback may reenter validation and release the caller's input leases. */
xls_semantic_completion* xlsSemanticCompletionTryWordView(xls_semantic_completion*,
    const struct xls_input_batch*,const char* queryKey,const struct xls_syntax* previousSyntax,
    const struct xls_syntax* currentSyntax,int (*cancelled)(void*),void* user);
#endif
