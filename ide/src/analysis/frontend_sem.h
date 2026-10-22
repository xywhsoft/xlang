#ifndef XLANG_ANALYSIS_FRONTEND_SEM_H
#define XLANG_ANALYSIS_FRONTEND_SEM_H
#include "analysis/text.h"
#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"
typedef struct xls_sem_memo xls_sem_memo;
typedef struct xls_sem_work { uint64_t checked,reused,contextHits,bytes; } xls_sem_work;
/* Private opt-in around the unchanged compiler checker. Memo facts are
 * immutable, independently owned, and never a substitute for whole-module
 * declaration checks, final type/region annotation or typed IR verification. */
int xlsFrontendSemCheckMemo(XL6_AstNode* root,XL6_DiagList* diags,const char* module,
    int captureIndex,const char* path,xls_text* text,const xls_sem_memo* previous,
    xls_sem_memo** result,xls_sem_work* work);
void xlsFrontendSemMemoDestroy(xls_sem_memo* memo);
typedef struct xls_sem_overload_work {
    uint64_t rootVisits,indexes,pairVisits,bytes;
} xls_sem_overload_work;
/* Private verification entry point: zero budget runs the unchanged pair scan.
 * The optional override/work record is scoped to this thread and call. */
int xlsFrontendSemCheckOverloads(XL6_AstNode* root,XL6_DiagList* diags,
    const char* module,int captureIndex,size_t bytes,xls_sem_overload_work* work);
#endif
