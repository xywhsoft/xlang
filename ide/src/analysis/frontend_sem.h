#ifndef XLANG_ANALYSIS_FRONTEND_SEM_H
#define XLANG_ANALYSIS_FRONTEND_SEM_H
#include "analysis/text.h"
#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"
#include "src/compile/xl6_source_report.h"
#include "analysis/ast_storage.h"
#include "analysis/ast_intern.h"
#include "analysis/frontend_symbols.h"
struct XL6_CompletionQuery;
typedef struct xls_sem_memo xls_sem_memo;
/* Exact non-coordinate AST identity for private raw IR fragments. Native
 * process-local sparse scalar batches;ordinary semantic key bytes unchanged.
 * Completion-source provenance is unsupported and declines caching. */
typedef enum xls_ir_key_mode {
    XLS_IR_KEY_EXACT,XLS_IR_KEY_MODULE,XLS_IR_KEY_FUNCTION,XLS_IR_KEY_ENVIRONMENT
} xls_ir_key_mode;
/* Lexical function regions and proven local declarations are relative;
 * fact-type trees and global/unproven symbols stay exact.
 * Environment mode ignores lexical regions unused by original type lookup.
 * Invalid/nonlocal function regions decline this optional cache key. */
int xlsFrontendIrAstKey(xstrbuf* key,const XL6_AstNode* node,xls_ir_key_mode mode);
int xlsFrontendIrAstKeySymbols(xstrbuf* key,const XL6_AstNode* node,const xls_lexical_symbols* symbols);
struct xls_entity_view;
/* Normalize only proven named symbol identities. Every unsupported scalar,
 * type,ABI and fact subtree remains exact;local/global/native domains differ.
 * Caller must separately prove equal entity store/domain before key reuse. */
int xlsFrontendIrAstKeyEntities(xstrbuf* key,const XL6_AstNode* node,
    const xls_lexical_symbols* symbols,const struct xls_entity_view* entities);
typedef struct xls_sem_storage_work { uint64_t nodes,pages,chargedBytes,heapFallbacks; } xls_sem_storage_work;
typedef struct xls_sem_storage_control {
    struct xls_sem_storage_control* previous;size_t budget;xls_sem_storage_work work;
} xls_sem_storage_control;
void xlsFrontendSemStorageBegin(xls_sem_storage_control* control,size_t bytes);
void xlsFrontendSemStorageEnd(xls_sem_storage_control* control,xls_sem_storage_work* work);
size_t xlsFrontendSemStorageBudget(void);
typedef struct xls_sem_tail xls_sem_tail;
typedef struct xls_sem_tail_work {
    uint64_t deferred,materialized,retainedBytes;
    uint64_t bodyFunctions,bodyMaterialized,bodyBytes,bodyFailures,bodyRetries;
} xls_sem_tail_work;
/* Completion/signature replies consume validated immutable function facts,
 * not their expanded body ASTs or final IR annotations. A private result may
 * retain independent unit/coordinate views and its exact type registry, then
 * materialize all bodies and the original passes before exposing its AST.
 * The two local quotas are registry<=8MiB and body views/jobs<=64MiB; neither
 * bounds the full model, temporary allocation, AST storage, or process RSS.
 * Ordinary/fresh checking remains eager. */
/* Returns zero on a temporary body materialization allocation failure. No
 * incomplete AST is exposed; query facts remain valid and a caller may retry. */
int xlsFrontendSemTailMaterialize(xls_sem_tail* tail);
void xlsFrontendSemTailDestroy(xls_sem_tail* tail);
xls_sem_tail_work xlsFrontendSemTailWork(const xls_sem_tail* tail);
typedef struct xls_sem_tail_control { struct xls_sem_tail_control* previous;size_t bytes; } xls_sem_tail_control;
void xlsFrontendSemTailBegin(xls_sem_tail_control* control,size_t bytes);
void xlsFrontendSemTailEnd(xls_sem_tail_control* control);

typedef struct xls_sem_body_control {struct xls_sem_body_control* previous;size_t bytes;} xls_sem_body_control;
/* Optional immutable function-view retention, at most 64 MiB of known data.
 * This includes each referenced unit/position capacity in full, plus jobs;
 * allocator/transient/whole-process storage is outside this local quota.
 * Zero retains the original eager fact restoration. Nested limits only shrink. */
void xlsFrontendSemBodyBegin(xls_sem_body_control* control,size_t bytes);
void xlsFrontendSemBodyEnd(xls_sem_body_control* control);
typedef struct xls_sem_work { uint64_t checked,reused,contextHits,bytes; } xls_sem_work;
typedef struct xls_sem_read_work {
    uint64_t contexts,contextHits,recorded,negative,matched,rejected,reused,bytes;
    uint64_t observations,typedComparisons,scratchReuses,scratchBytes,valueBytes;
} xls_sem_read_work;
/* Real compiler declaration-lookup observations,not a complete semantic query
 * database. All original non-function/allocation/type/source/restore proofs
 * still apply. Header snapshots include live semantic facts;negative queries
 * record an empty candidate set. Unknown observations disable relaxed reuse. */
xls_sem_read_work xlsFrontendSemMemoReads(const xls_sem_memo* memo);
typedef struct xls_sem_read_control {struct xls_sem_read_control* previous;size_t bytes;} xls_sem_read_control;
void xlsFrontendSemReadsBegin(xls_sem_read_control* control,size_t bytes);
void xlsFrontendSemReadsEnd(xls_sem_read_control* control);
/* Optional transient buffer retention,not proof admission. Zero retains no
 * scratch bytes and runs every serialized comparison through fresh buffers.
 * Nested scopes can only reduce the 8 MiB retention allowance. */
typedef struct xls_sem_scratch_control {struct xls_sem_scratch_control* previous;size_t bytes;} xls_sem_scratch_control;
void xlsFrontendSemScratchBegin(xls_sem_scratch_control* control,size_t bytes);
void xlsFrontendSemScratchEnd(xls_sem_scratch_control* control);
typedef struct xls_sem_value_control {struct xls_sem_value_control* previous;int enabled;} xls_sem_value_control;
/* Private verification fallback: serialized comparisons remain authoritative
 * and always validate each distinct snapshot after the function finishes. */
void xlsFrontendSemValuesBegin(xls_sem_value_control* control,int enabled);
void xlsFrontendSemValuesEnd(xls_sem_value_control* control);
/* Private opt-in around the unchanged compiler checker. Memo facts are
 * immutable, independently owned, and never a substitute for whole-module
 * declaration checks, final type/region annotation or typed IR verification.
 * Reports borrow original captured source bytes for this call only. Changed
 * source pairs use bounded temporary text indexes; memos retain neither a
 * report, its text, nor an earlier result. Default fresh checks do not opt in. */
/* Proven native local-allocation intervals can rebind current symbol facts;
 * immutable units retain their original origin. Type identities,predeclared
 * context,foreign dynamic facts and non-function boundaries stay guarded. */
int xlsFrontendSemCheckMemo(XL6_AstNode* root,XL6_DiagList* diags,const char* module,
    int captureIndex,const char* path,const XL6_SourceReport* sources,
    const XL6_SourceReport* previousSources,const xls_sem_memo* previous,
    xls_sem_memo** result,xls_sem_work* work);
void xlsFrontendSemMemoDestroy(xls_sem_memo* memo);
/* Static memo forest representation,not current-query work or RSS.
 * Shared units are charged once per entry,including on restored views.
 * Foreground cursor/signature memos use the original independent facts and
 * report zero forest provenance. No extra traversal on their hot path. */
xls_ast_intern_work xlsFrontendSemMemoTypes(const xls_sem_memo* memo);
/* Cursor mode retains only query-neutral functions. Query-source siblings
 * additionally prove exact source/marker identity and range/text exclusion;
 * their immutable units own the original marker stamp. Explicit private clone
 * admission binds those facts to current metadata. Projection state must stay
 * unchanged across the original check before facts can be published. */
int xlsFrontendSemCheckCursorMemo(XL6_AstNode* root,XL6_DiagList* diags,const char* module,
    struct XL6_CompletionQuery* query,const char* entry,const XL6_SourceReport* sources,
    const XL6_SourceReport* previousSources,const xls_sem_memo* previous,
    xls_sem_memo** result,xls_sem_work* work);
int xlsFrontendSemCheckCursorDeferred(XL6_AstNode* root,XL6_DiagList* diags,const char* module,
    struct XL6_CompletionQuery* query,const char* entry,const XL6_SourceReport* sources,
    const XL6_SourceReport* previousSources,const xls_sem_memo* previous,
    xls_sem_memo** result,xls_sem_work* work,xls_sem_tail** tail);
typedef struct xls_sem_overload_work {
    uint64_t rootVisits,indexes,pairVisits,bytes;
} xls_sem_overload_work;
typedef struct xls_sem_candidate_scope {
    struct xls_sem_candidate_scope* previous;
    size_t budget;
    xls_sem_overload_work work;
} xls_sem_candidate_scope;
/* Private same-thread verification scope. End restores the previous override;
 * candidate data remains owned by each individual compiler enumeration.
 * The same test allowance independently bounds its read-only alias vector. */
void xlsFrontendSemCandidatesBegin(xls_sem_candidate_scope* scope,size_t bytes);
void xlsFrontendSemCandidatesEnd(xls_sem_candidate_scope* scope,xls_sem_overload_work* work);
/* Call resolution keeps original predicates/diagnostics and declaration order.
 * A zero-byte same-thread control runs the original root scan for comparison.
 * Controls are separate from conflict/completion-enumeration accounting. */
typedef struct xls_sem_call_work {
    uint64_t indexes,rootVisits,indexedQueries,linearQueries,candidateVisits,bytes;
} xls_sem_call_work;
typedef struct xls_sem_call_control {
    struct xls_sem_call_control* previous;
    size_t budget;
    xls_sem_call_work work;
} xls_sem_call_control;
void xlsFrontendSemCallsBegin(xls_sem_call_control* scope,size_t bytes);
void xlsFrontendSemCallsEnd(xls_sem_call_control* scope,xls_sem_call_work* work);
size_t xlsFrontendSemCallsBudget(void);
/* Private verification entry point: zero budget runs the unchanged pair scan.
 * The optional override/work record is scoped to this thread and call. */
int xlsFrontendSemCheckOverloads(XL6_AstNode* root,XL6_DiagList* diags,
    const char* module,int captureIndex,size_t bytes,xls_sem_overload_work* work);
#endif
