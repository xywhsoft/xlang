#ifndef XLANG_ANALYSIS_FRONTEND_SEM_H
#define XLANG_ANALYSIS_FRONTEND_SEM_H
#include "analysis/text.h"
#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"
#include "src/compile/xl6_source_report.h"
typedef struct xls_sem_memo xls_sem_memo;
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
/* Private verification entry point: zero budget runs the unchanged pair scan.
 * The optional override/work record is scoped to this thread and call. */
int xlsFrontendSemCheckOverloads(XL6_AstNode* root,XL6_DiagList* diags,
    const char* module,int captureIndex,size_t bytes,xls_sem_overload_work* work);
#endif
