/* Included after the original final annotation helpers. The build adapter
 * extracts their exact original invocation block into xlsSemFinalizeAnnotations.
 * Only the private retained completion route can defer it. No language rules,
 * diagnostics or ordinary-check/IR annotations are omitted. */
#define XLS_SEM_TAIL_BYTES (8u*1024u*1024u)
struct xls_sem_tail {
    XL6_AstNode* root; /* borrowed from the same private result */
    XL6_TypeRegistry types; /* moved, never copied or shared */
    size_t bytes;
    int materialized;
};
static _Thread_local xls_sem_tail_control* xlsActiveTailControl;
void xlsFrontendSemTailBegin(xls_sem_tail_control* control,size_t bytes)
{
    if(!control) return;
    control->previous=xlsActiveTailControl;control->bytes=bytes;
    xlsActiveTailControl=control;
}
void xlsFrontendSemTailEnd(xls_sem_tail_control* control)
{if(control && xlsActiveTailControl==control)xlsActiveTailControl=control->previous;}
static int xlsSemTailCharge(size_t* bytes,size_t amount,size_t budget)
{if(amount>budget-*bytes)return 0;*bytes+=amount;return 1;}
static int xlsSemDeferAnnotations(XL6_SemContext* ctx,XL6_AstNode* root)
{
    xls_sem_build* build=xlsActiveSemBuild;
    const XL6_CompletionQuery* query=ctx->completionQuery;
    size_t budget=xlsActiveTailControl ? xlsActiveTailControl->bytes : XLS_SEM_TAIL_BYTES;
    if(budget>XLS_SEM_TAIL_BYTES)budget=XLS_SEM_TAIL_BYTES;
    /* A rejected/error projection stays on the original eager route. The
     * final helpers cannot diagnose or change the already captured query. */
    if(!build || !build->deferred || *build->deferred || !root || ctx->root!=root ||
       build->deferredRoot!=root || build->deferredQuery!=query || ctx->captureIndex ||
       !query || query->parsed!=1 || query->reached!=1 || !query->available || query->error ||
       (ctx->diags && ctx->diags->count) || budget<sizeof(xls_sem_tail))return 0;
    size_t bytes=sizeof(xls_sem_tail);
    if(ctx->types.capacity>(budget-bytes)/sizeof(*ctx->types.items) ||
       !xlsSemTailCharge(&bytes,ctx->types.capacity*sizeof(*ctx->types.items),budget) ||
       ctx->types.slotCapacity>(budget-bytes)/sizeof(*ctx->types.slots) ||
       !xlsSemTailCharge(&bytes,ctx->types.slotCapacity*sizeof(*ctx->types.slots),budget))return 0;
    for(size_t i=0;i<ctx->types.count;++i) {
        const XL6_TypeInfo* type=&ctx->types.items[i];
        if((type->name && !xlsSemTailCharge(&bytes,strlen(type->name)+1,budget)) ||
           (type->abiName && !xlsSemTailCharge(&bytes,strlen(type->abiName)+1,budget)))return 0;
    }
    xls_sem_tail* tail=xrtCalloc(1,sizeof(*tail));if(!tail)return 0;
    tail->root=root;tail->types=ctx->types;tail->bytes=bytes;
    memset(&ctx->types,0,sizeof(ctx->types));*build->deferred=tail;
    return 1;
}
void xlsFrontendSemTailMaterialize(xls_sem_tail* tail)
{
    if(!tail || tail->materialized)return;
    /* Audited original passes read only root/types from this context. Registry
     * declarations point into the owned root, not retired semantic scratch.
     * The result mutex serializes this one transition before AST exposure. */
    XL6_SemContext ctx={0};ctx.root=tail->root;ctx.types=tail->types;
    xlsSemFinalizeAnnotations(&ctx,tail->root);
    xl6TypeRegistryFree(&tail->types);tail->bytes=sizeof(*tail);tail->materialized=1;
}
void xlsFrontendSemTailDestroy(xls_sem_tail* tail)
{if(tail){xl6TypeRegistryFree(&tail->types);xrtFree(tail);}}
xls_sem_tail_work xlsFrontendSemTailWork(const xls_sem_tail* tail)
{return tail ? (xls_sem_tail_work){1,(uint64_t)tail->materialized,tail->bytes} : (xls_sem_tail_work){0};}
