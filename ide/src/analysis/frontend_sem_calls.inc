/* Included immediately before the two original call resolvers. The audited
 * shared semantic TU never changes module declaration membership or function
 * names while checking bodies. Signatures/types/bodies remain mutable and are
 * read by the original predicates on every candidate, never cached here. */
#include "analysis/frontend_sem_names.inc"
typedef struct xls_sem_call_scope {
    struct xls_sem_call_scope* previous;
    XL6_SemContext* context;
    const XL6_AstNode* root;
    XL6_AstNode** children;
    size_t count;
    xmap heads;
    xls_sem_overload_scan index;
    xls_sem_call_control* control;
    xls_sem_call_work work;
    int disabled;
} xls_sem_call_scope;
static _Thread_local xls_sem_call_scope* xlsActiveCallScope;
static _Thread_local xls_sem_call_control* xlsActiveCallControl;
size_t xlsFrontendSemCallsBudget(void)
{return xlsActiveCallControl ? xlsActiveCallControl->budget : XLS_SEM_NAMES_BYTES;}
static void xlsSemCallWorkAdd(xls_sem_call_work* out,const xls_sem_call_work* work)
{
    out->indexes+=work->indexes;out->rootVisits+=work->rootVisits;
    out->indexedQueries+=work->indexedQueries;out->linearQueries+=work->linearQueries;
    out->candidateVisits+=work->candidateVisits;
    if(work->bytes>out->bytes)out->bytes=work->bytes;
}
void xlsFrontendSemCallsBegin(xls_sem_call_control* scope,size_t bytes)
{
    memset(scope,0,sizeof(*scope));scope->previous=xlsActiveCallControl;
    scope->budget=bytes;xlsActiveCallControl=scope;
}
void xlsFrontendSemCallsEnd(xls_sem_call_control* scope,xls_sem_call_work* work)
{
    if(!scope || xlsActiveCallControl!=scope) return;
    xlsActiveCallControl=scope->previous;if(work)*work=scope->work;
    if(scope->previous)xlsSemCallWorkAdd(&scope->previous->work,&scope->work);
    memset(scope,0,sizeof(*scope));
}
static void xlsSemCallsBegin(xls_sem_call_scope* scope,XL6_SemContext* context)
{
    memset(scope,0,sizeof(*scope));scope->previous=xlsActiveCallScope;
    scope->context=context;scope->root=context->root;scope->children=context->root->children;scope->count=context->root->childCount;
    scope->control=xlsActiveCallControl;
    xls_sem_overload_work built={0};
    scope->index=xlsSemNamesBuild(scope->root,&scope->heads,
        scope->control ? scope->control->budget : XLS_SEM_NAMES_BYTES,&built);
    scope->index.work=NULL; /* The temporary builder work has ended. */
    scope->work.indexes=built.indexes;scope->work.rootVisits=built.rootVisits;scope->work.bytes=built.bytes;
    xlsActiveCallScope=scope;
}
static void xlsSemCallsEnd(xls_sem_call_scope* scope)
{
    if(xlsActiveCallScope!=scope) return;
    xlsActiveCallScope=scope->previous;
    if(scope->control) {
        xlsSemCallWorkAdd(&scope->control->work,&scope->work);
    }
    xrtFree(scope->index.next);xrtMapUnit(&scope->heads);
}
static int xlsSemCallsReady(xls_sem_call_scope* scope,XL6_SemContext* context)
{
    if(!scope || scope->context!=context) return 0;
    if(scope->root!=context->root || scope->children!=context->root->children || scope->count!=context->root->childCount)
        scope->disabled=1;
    return !scope->disabled && scope->index.next;
}
static size_t xlsSemCallFirst(XL6_SemContext* context,const char* name)
{
    xls_sem_call_scope* scope=xlsActiveCallScope;size_t first=0;
    if(xlsSemCallsReady(scope,context)) {
        size_t* found=xrtMapGet(&scope->heads,(xbytesview){(const unsigned char*)name,strlen(name)});
        first=found ? *found : context->root->childCount;++scope->work.indexedQueries;
    } else if(scope && scope->context==context) ++scope->work.linearQueries;
    if(scope && scope->context==context && first<context->root->childCount)++scope->work.candidateVisits;
    return first;
}
static size_t xlsSemCallNext(XL6_SemContext* context,size_t previous)
{
    xls_sem_call_scope* scope=xlsActiveCallScope;
    size_t next=xlsSemCallsReady(scope,context) ? scope->index.next[previous] : previous+1;
    if(scope && scope->context==context && next<context->root->childCount)++scope->work.candidateVisits;
    return next;
}
