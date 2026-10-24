#ifndef XLANG_ANALYSIS_FRONTEND_SEM_H
#define XLANG_ANALYSIS_FRONTEND_SEM_H
#include "analysis/text.h"
#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"
#include "src/compile/xl6_source_report.h"
struct XL6_CompletionQuery;
typedef struct xls_sem_memo xls_sem_memo;
typedef struct xls_sem_tail xls_sem_tail;
typedef struct xls_sem_tail_work { uint64_t deferred,materialized,retainedBytes; } xls_sem_tail_work;
/* Completion replies do not consume final IR annotations. A private result may
 * retain the exact registry and materialize the original passes once, before
 * exposing its full AST. Ordinary/fresh checking remains eager. */
void xlsFrontendSemTailMaterialize(xls_sem_tail* tail);
void xlsFrontendSemTailDestroy(xls_sem_tail* tail);
xls_sem_tail_work xlsFrontendSemTailWork(const xls_sem_tail* tail);
typedef struct xls_sem_tail_control { struct xls_sem_tail_control* previous;size_t bytes; } xls_sem_tail_control;
void xlsFrontendSemTailBegin(xls_sem_tail_control* control,size_t bytes);
void xlsFrontendSemTailEnd(xls_sem_tail_control* control);
typedef struct xls_sem_work { uint64_t checked,reused,contextHits,bytes; } xls_sem_work;
/* Private opt-in around the unchanged compiler checker. Memo facts are
 * immutable, independently owned, and never a substitute for whole-module
 * declaration checks, final type/region annotation or typed IR verification.
 * Reports borrow original captured source bytes for this call only. Changed
 * source pairs use bounded temporary text indexes; memos retain neither a
 * report, its text, nor an earlier result. Default fresh checks do not opt in. */
int xlsFrontendSemCheckMemo(XL6_AstNode* root,XL6_DiagList* diags,const char* module,
    int captureIndex,const char* path,const XL6_SourceReport* sources,
    const XL6_SourceReport* previousSources,const xls_sem_memo* previous,
    xls_sem_memo** result,xls_sem_work* work);
void xlsFrontendSemMemoDestroy(xls_sem_memo* memo);
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
