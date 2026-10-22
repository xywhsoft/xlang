/* Included immediately before the shared overload-conflict scan. Only its
 * iteration candidates change; original name/signature tests and diagnostic
 * order remain in the compiler body. No names, declarations or root membership
 * are mutated by that scan or its signature/type/alias predicates. Completion
 * name enumeration borrows the same declaration membership only while its
 * original visibility, type generation and add/filter bodies run; it never
 * spans checking another function body. No prefix or type error is skipped. */
#define XLS_SEM_OVERLOAD_BYTES (8u*1024u*1024u)
typedef struct xls_sem_overload_scan {
    size_t* next;
    xls_sem_overload_work* work;
} xls_sem_overload_scan;
static _Thread_local xls_sem_candidate_scope* xlsActiveOverloadControl;

static xls_sem_overload_scan xlsSemOverloadBegin(const XL6_AstNode* root,xmap* heads)
{
    xls_sem_candidate_scope* control=xlsActiveOverloadControl;
    xls_sem_overload_scan scan={NULL,control ? &control->work : NULL};
    size_t budget=control ? control->budget : XLS_SEM_OVERLOAD_BYTES;
    if(budget>XLS_SEM_OVERLOAD_BYTES) budget=XLS_SEM_OVERLOAD_BYTES;
    /* Small modules keep the allocation-free original loop. The charged
     * allowance includes the vector and conservative XRT map/key overhead;
     * it is an auxiliary-data bound, not a process RSS measurement. */
    if(!root || root->childCount<64 || budget<256 ||
       root->childCount>(budget-256)/sizeof(size_t)) return scan;
    size_t charged=root->childCount*sizeof(size_t)+256;
    size_t* next=xrtMalloc(root->childCount*sizeof(size_t));
    xmap names={0};
    if(!next || !xrtMapInit(&names,sizeof(size_t))) { xrtFree(next); return scan; }
    int okay=1;
    for(size_t i=root->childCount;i-- && okay;) {
        if(scan.work) ++scan.work->rootVisits;
        next[i]=root->childCount;
        const XL6_AstNode* node=root->children[i];
        const char* name=xl6SemIsFuncDecl(node) ? xl6SemFuncName(node) : NULL;
        if(!name) continue; /* Original xl6TextEquals never matches NULL. */
        size_t size=strlen(name);
        if(size>256) { okay=0; break; }
        xbytesview key={(const unsigned char*)name,size};
        size_t* later=xrtMapGet(&names,key);
        if(later) next[i]=*later;
        else {
            size_t add=size+1+96;
            if(add>budget-charged) { okay=0; break; }
            charged+=add;
        }
        if(!xrtMapSet(&names,key,&i)) okay=0;
    }
    /* Completion enumerates several original symbol names. Transfer the same
     * bounded map to that caller; conflict checking needs only next links. */
    if(okay && heads) { *heads=names; memset(&names,0,sizeof(names)); }
    xrtMapUnit(&names);
    if(!okay) { xrtFree(next); return scan; }
    scan.next=next;
    if(scan.work) { ++scan.work->indexes; if(charged>scan.work->bytes) scan.work->bytes=charged; }
    return scan;
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
    xlsFrontendSemCandidatesEnd(&control,work);
    return okay;
}
