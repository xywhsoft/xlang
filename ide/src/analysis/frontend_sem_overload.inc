/* Included immediately before the shared overload-conflict scan. Only its
 * iteration candidates change; original name/signature tests and diagnostic
 * order remain in the compiler body. No names, declarations or root membership
 * are mutated by that scan or its signature/type/alias predicates. Completion
 * name enumeration borrows the same declaration membership only while its
 * original visibility, type generation and add/filter bodies run; it never
 * spans checking another function body. No prefix or type error is skipped. */
#include "analysis/frontend_types.h"
#define XLS_SEM_OVERLOAD_BYTES (8u*1024u*1024u)
static _Thread_local xls_sem_candidate_scope* xlsActiveOverloadControl;

static void xlsSemCompletionTypesBegin(xls_type_alias_scope* scope,const XL6_AstNode* root)
{
    /* Name enumeration only reads module declarations. Callable signatures
     * and value-name inference own their temporary nodes; no alias or root
     * membership changes here. End before returning to the mutable checker.
     * Reuse the existing alias selector, including its bounded fallback. */
    size_t budget=xlsActiveOverloadControl ? xlsActiveOverloadControl->budget : XLS_SEM_OVERLOAD_BYTES;
    xlsFrontendTypeAliasBegin(scope,root,budget);
}

static xls_sem_overload_scan xlsSemOverloadBegin(const XL6_AstNode* root,xmap* heads)
{
    xls_sem_candidate_scope* control=xlsActiveOverloadControl;
    return xlsSemNamesBuild(root,heads,control ? control->budget : XLS_SEM_OVERLOAD_BYTES,control ? &control->work : NULL);
}
static size_t xlsSemOverloadFirst(xls_sem_overload_scan* scan,xmap* heads,const char* name,size_t count)
{
    size_t first=0;
    if(scan->next) {
        size_t* head=name ? xrtMapGet(heads,(xbytesview){(const unsigned char*)name,strlen(name)}) : NULL;
        first=head ? *head : count;
    }
    if(scan->work && first<count) ++scan->work->pairVisits;
    return first;
}
static size_t xlsSemOverloadNext(xls_sem_overload_scan* scan,size_t previous,size_t count)
{
    size_t next=scan->next ? scan->next[previous] : previous+1;
    if(scan->work && next<count) ++scan->work->pairVisits;
    return next;
}
static void xlsSemOverloadEnd(xls_sem_overload_scan* scan)
{ xrtFree(scan->next); scan->next=NULL; }
void xlsFrontendSemCandidatesBegin(xls_sem_candidate_scope* scope,size_t bytes)
{
    memset(scope,0,sizeof(*scope)); scope->previous=xlsActiveOverloadControl;
    scope->budget=bytes; xlsActiveOverloadControl=scope;
}
void xlsFrontendSemCandidatesEnd(xls_sem_candidate_scope* scope,xls_sem_overload_work* work)
{
    if(!scope || xlsActiveOverloadControl!=scope) return;
    xlsActiveOverloadControl=scope->previous;
    if(work) *work=scope->work;
    memset(scope,0,sizeof(*scope));
}

int xlsFrontendSemCheckOverloads(XL6_AstNode* root,XL6_DiagList* diags,
    const char* module,int captureIndex,size_t bytes,xls_sem_overload_work* work)
{
    xls_sem_candidate_scope control;
    xlsFrontendSemCandidatesBegin(&control,bytes);
    int okay=xl6SemCheckModuleWithIndex(root,diags,module,captureIndex);
    /* This structured call owns its local override. Always restore it before
     * the stack frame ends, including after nested checker invocations. */
    xlsActiveOverloadControl=control.previous;
    if(work) *work=control.work;
    return okay;
}
