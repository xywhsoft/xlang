/* Included in the native semantic TU after XL6_SemContext. Observe its one
 * original allocator; never change compiler IDs,allocation order or errors. */
typedef struct xls_sem_symbol_span {
    XL6_SymbolId start,end,floor;
    int local;
} xls_sem_symbol_span;
_Static_assert(sizeof(XL6_SymbolId)==sizeof(uint32_t) && XL6_SYMBOL_ID_INVALID==0,"audit native symbol ID domain");
typedef struct xls_sem_symbol_scope {
    struct xls_sem_symbol_scope* previous;
    const XL6_SemContext* context;
    const XL6_AstNode* function;
    XL6_SymbolId start,next,floor;
    int local;
} xls_sem_symbol_scope;
static _Thread_local xls_sem_symbol_scope* xlsActiveSymbols;
static XL6_SymbolId xlsSemReserveSymbolIdOriginal(XL6_SemContext*,XL6_AstNode*);
static XL6_SymbolId xl6SemReserveSymbolId(XL6_SemContext* ctx,XL6_AstNode* node)
{
    XL6_SymbolId id=xlsSemReserveSymbolIdOriginal(ctx,node);
    xls_sem_symbol_scope* scope=xlsActiveSymbols;
    if(scope && scope->context==ctx){
        const XL6_AstNode* root=scope->function;
        const char *a=xl6AstSourcePath(root),*b=xl6AstSourcePath(node);
        if(!id || id!=scope->next || id==UINT32_MAX || ctx->nextSymbolId!=id+1 ||
           !node || !a || !b || !*a || !*b || (a!=b && strcmp(a,b)) || !node->range.known ||
           node->range.endOffset<node->range.startOffset ||
           node->range.startOffset<root->range.startOffset || node->range.endOffset>root->range.endOffset)scope->local=0;
        scope->next=ctx->nextSymbolId;
    }
    return id;
}
static void xlsSemSymbolsBegin(xls_sem_symbol_scope* scope,const XL6_SemContext* ctx,
    const XL6_AstNode* function,XL6_SymbolId floor)
{
    *scope=(xls_sem_symbol_scope){.previous=xlsActiveSymbols,.context=ctx,.function=function,
        .start=ctx->nextSymbolId,.next=ctx->nextSymbolId,.floor=floor,
        .local=ctx->nextSymbolId && floor && ctx->nextSymbolId>=floor && function &&
            xl6AstSourcePath(function) && *xl6AstSourcePath(function) && function->range.known && function->range.endOffset>=function->range.startOffset};
    xlsActiveSymbols=scope;
}
static xls_sem_symbol_span xlsSemSymbolsEnd(xls_sem_symbol_scope* scope,const XL6_SemContext* ctx)
{
    xls_sem_symbol_span span={scope->start,ctx->nextSymbolId,scope->floor,
        scope->local && scope->next==ctx->nextSymbolId && ctx->nextSymbolId>=scope->start};
    xlsActiveSymbols=scope->previous;return span;
}
static int xlsSemSymbolsLimit(const xls_sem_symbol_span* span,XL6_SymbolId start,XL6_SymbolId* end)
{
    if(!start || !span->floor || span->start<span->floor || span->end<span->start || span->end-span->start>UINT32_MAX-start)return 0;
    *end=start+(span->end-span->start);return 1;
}
static int xlsSemSymbolsBind(const xls_sem_symbol_span* span,XL6_SymbolId start,int changed,XL6_SymbolId* symbol)
{
    if(!changed || !*symbol || *symbol<span->floor)return 1;
    /* Every ID in this interval came from an observed,source-proven local
     * declaration. Dynamic IDs from other functions/classes are uncertain,
     * even if the current counter happens to match after several edits. */
    if(!span->local || *symbol<span->start || *symbol>=span->end)return 0;
    uint64_t current=(uint64_t)start+(*symbol-span->start);
    if(!current || current>UINT32_MAX)return 0;
    *symbol=(XL6_SymbolId)current;return 1;
}
