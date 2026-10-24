/* Shared bounded declaration-name candidate builder. Included only in the
 * audited semantic TU, after original IsFuncDecl/FuncName helpers. It borrows
 * no mutable function signatures and changes no declaration/diagnostic order.
 * Call and conflict/completion scopes own their indexes independently. */
#define XLS_SEM_NAMES_BYTES (8u*1024u*1024u)
typedef struct xls_sem_overload_scan {
    size_t* next;
    xls_sem_overload_work* work;
} xls_sem_overload_scan;
static xls_sem_overload_scan xlsSemNamesBuild(const XL6_AstNode* root,xmap* heads,size_t budget,xls_sem_overload_work* work)
{
    xls_sem_overload_scan scan={NULL,work};
    if(budget>XLS_SEM_NAMES_BYTES) budget=XLS_SEM_NAMES_BYTES;
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
