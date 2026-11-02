/* Complete immutable-forest proof, never a content/identity hash. Types cannot
 * acquire coordinates or sources after unit publication. Current
 * source/cursor ownership still has to pass the original clone predicate.
 * Unsupported trees and optional allocation failures keep the recursive path.
 * The unit owns all source objects referenced here. */
#define XLS_SEM_PROOF_BYTES 4096u
#define XLS_SEM_PROOF_SOURCES 64u
static _Thread_local xls_sem_proof_control* xlsActiveProofControl;
static size_t xlsSemProofBudget(void)
{
    size_t n=xlsActiveProofControl ? xlsActiveProofControl->bytes : XLS_SEM_PROOF_BYTES;
    return n<XLS_SEM_PROOF_BYTES ? n : XLS_SEM_PROOF_BYTES;
}
void xlsFrontendSemProofBegin(xls_sem_proof_control* control,size_t bytes)
{
    if(!control)return;
    size_t available=xlsSemProofBudget();if(bytes>available)bytes=available;
    *control=(xls_sem_proof_control){xlsActiveProofControl,bytes};xlsActiveProofControl=control;
}
void xlsFrontendSemProofEnd(xls_sem_proof_control* control)
{if(control && xlsActiveProofControl==control)xlsActiveProofControl=control->previous;}
static void xlsSemProofDestroy(xls_sem_proof* proof)
{if(proof){xrtArrayUnit(&proof->sources);xrtArrayUnit(&proof->located);xrtFree(proof);}}
static int xlsSemProofFits(xls_sem_proof* proof,size_t limit)
{
    if(limit<sizeof(*proof) || proof->sources.Capacity>(limit-sizeof(*proof))/sizeof(const XL6_AstSource*))return 0;
    size_t bytes=sizeof(*proof)+proof->sources.Capacity*sizeof(const XL6_AstSource*);
    if(proof->located.Capacity>(limit-bytes)/sizeof(const XL6_AstNode*))return 0;
    proof->bytes=bytes+proof->located.Capacity*sizeof(const XL6_AstNode*);return 1;
}
static int xlsSemProofCharge(xls_sem_proof* proof,size_t bytes)
{
    if(bytes>XLS_SEM_MEMO_BYTES-proof->cloneBytes)return 0;
    proof->cloneBytes+=bytes;return 1;
}
static void xlsSemProofSymbol(xls_sem_proof* proof,const xls_sem_symbol_span* span,XL6_SymbolId symbol)
{
    if(symbol && symbol>=span->floor &&
       (!span->local || symbol<span->start || symbol>=span->end))proof->shiftSafe=0;
}
static int xlsSemProofType(xls_sem_proof* proof,const xls_sem_symbol_span* span,
    const XL6_AstNode* node,size_t depth,size_t limit)
{
    if(!node)return 1;
    if(depth>512 ||
       node->childCount>XLS_SEM_MEMO_BYTES/sizeof(*node->children) ||
       (node->childCount && !node->children) || !xlsSemProofCharge(proof,sizeof(*node)) ||
       !xlsSemProofCharge(proof,node->childCount*sizeof(*node->children)))return 0;
    if((node->text && !xlsSemProofCharge(proof,strlen(node->text)+1)) ||
       (node->doc && !xlsSemProofCharge(proof,strlen(node->doc)+1)) ||
       (node->literalData && (node->literalSize==SIZE_MAX || !xlsSemProofCharge(proof,node->literalSize+1))))return 0;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    if(node->compiledPackageLink && !xlsSemProofCharge(proof,strlen(node->compiledPackageLink)+1))return 0;
    xlsSemProofSymbol(proof,span,node->semanticReceiverSymbolId);
#endif
    ++proof->nodes;
    if((node->line || node->column || node->range.known || node->tokenRange.known) &&
       (!xrtArrayPush(&proof->located,&node) || !xlsSemProofFits(proof,limit)))return 0;
    XL6_SymbolId ids[]={node->semanticSymbolId,node->semanticDeclaredSymbolId,
        node->semanticTargetSymbolId,node->semanticCompoundOperatorSymbolId};
    for(size_t i=0;i<4;++i)xlsSemProofSymbol(proof,span,ids[i]);
    if(node->source){
        const XL6_AstSource* source=node->source;
        size_t length=source->path ? strlen(source->path)+1 : 0;
        if(length>SIZE_MAX-sizeof(*source) || !xlsSemProofCharge(proof,sizeof(*source)+length))return 0;
        size_t at=0;
        for(;at<proof->sources.Count;++at)
            if(*(const XL6_AstSource* const*)xrtArrayConstGet(&proof->sources,at)==source)break;
        if(at==proof->sources.Count){
            if(at>=XLS_SEM_PROOF_SOURCES || !xrtArrayPush(&proof->sources,&source) || !xlsSemProofFits(proof,limit))return 0;
        }
    }
    for(size_t i=0;i<node->childCount;++i)
        if(!xlsSemProofType(proof,span,node->children[i],depth+1,limit))return 0;
    return xlsSemProofType(proof,span,node->inferredType,depth+1,limit) &&
        xlsSemProofType(proof,span,node->semanticDeclaredType,depth+1,limit);
}
static xls_sem_proof* xlsSemProofCreate(const xls_sem_unit* unit,size_t limit)
{
    size_t budget=xlsSemProofBudget();if(limit>budget)limit=budget;
    if(!unit || limit<sizeof(xls_sem_proof) ||
       unit->facts.Count>XLS_SEM_MEMO_BYTES/(2*sizeof(XL6_AstNode*)))return NULL;
    xls_sem_proof* proof=xrtCalloc(1,sizeof(*proof));if(!proof)return NULL;
    proof->shiftSafe=1;proof->facts=unit->facts.Count;proof->cloneBytes=sizeof(XL6_AstNode);
    if(!xrtArrayInit(&proof->sources,sizeof(const XL6_AstSource*)) || !xrtArrayInit(&proof->located,sizeof(const XL6_AstNode*)) ||
       !xlsSemProofCharge(proof,2*unit->facts.Count*sizeof(XL6_AstNode*)))goto failed;
    for(size_t i=0;i<unit->facts.Count;++i){
        const xls_sem_fact* fact=xrtArrayConstGet(&unit->facts,i);
        XL6_SymbolId ids[]={fact->symbol,fact->declared,fact->target,fact->compound};
        for(size_t j=0;j<4;++j)xlsSemProofSymbol(proof,&unit->symbols,ids[j]);
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
        xlsSemProofSymbol(proof,&unit->symbols,fact->receiver);
        if(fact->packageLink && !xlsSemProofCharge(proof,strlen(fact->packageLink)+1))goto failed;
#endif
        if(!xlsSemProofType(proof,&unit->symbols,fact->inferred,1,limit) ||
           !xlsSemProofType(proof,&unit->symbols,fact->declaredType,1,limit))goto failed;
    }
    if(!xlsSemProofFits(proof,limit))goto failed;
    return proof;
failed:
    xlsSemProofDestroy(proof);return NULL;
}
