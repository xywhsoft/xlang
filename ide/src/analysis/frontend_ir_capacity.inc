/* Capacity hints only; no semantic/cache admission and no retained storage.
 * A current completed function-input identity bounds the unchanged raw forest.
 * Use its existing memo/proof pair, never the previous whole module's size:
 * an edited large function can shrink while keeping the same declaration ID.
 * Ordinary growth remains authoritative after a declined/failed reservation. */
#ifndef XLS_IR_CAPACITY_EVENT
#define XLS_IR_CAPACITY_EVENT(begin,bytes,okay) ((void)0)
#endif
static void xlsIrCapacityReserve(void** items,size_t* capacity,size_t count,size_t itemSize)
{
    if(!items || !capacity || !itemSize || count<=*capacity)return;
    size_t next=*capacity ? *capacity : 64;
    while(next<count){if(next>SIZE_MAX/2)return;next*=2;}
    if(next>SIZE_MAX/itemSize)return;
    XLS_IR_CAPACITY_EVENT(1,next*itemSize,0);
    void* grown=xrtRealloc(*items,next*itemSize);
    XLS_IR_CAPACITY_EVENT(0,next*itemSize,grown!=NULL);
    if(grown){*items=grown;*capacity=next;}
}
static int xlsIrCapacityAdd(const xls_ir_unit* unit,const xls_ir_input_proof* proof,
    const xls_typed_input_identity* identity,size_t* nodes,size_t* children)
{
    if(!unit || !proof || !identity || proof->identity!=identity || !unit->nodes ||
       *nodes>UINT32_MAX || unit->nodes>UINT32_MAX-*nodes || unit->children>SIZE_MAX-*children)return 0;
    *nodes+=unit->nodes;*children+=unit->children;return 1;
}
static void xlsIrCapacityForest(xls_ir_memo_build* build,XL6_IrModule* module,
    const XL6_AstNode* root,const XL6_AstNode* parent)
{
    xls_ir_input_scope* scope=xlsActiveIrInput;
    if(!build || build->module!=module || build->root!=root || parent ||
       !module || module->nodeCount || module->childCount || !root || root->kind!=XL6_AST_MODULE ||
       !scope || scope->root!=root || !scope->semantic || !scope->input || !build->baseline || !build->memo || xlsFrontendCancelObserved() ||
       root->childCount!=build->memo->entries.Count || (root->childCount && !root->children))return;
    size_t nodes=1,children=root->childCount;int matched=0;
    for(size_t i=0;i<root->childCount;++i){
        if((i&63u)==0 && xlsFrontendCancelPoll())return;
        const XL6_AstNode* source=root->children[i];
        if(!source || (source->kind!=XL6_AST_FUNC && source->kind!=XL6_AST_ASYNC_FUNC && source->kind!=XL6_AST_EXPORT_FUNC))continue;
        uint64_t entity=xlsEntityViewSymbol(build->memo->entities,source->semanticDeclaredSymbolId);
        size_t slot=SIZE_MAX;const xls_ir_unit* unit=xlsIrPreviousFunctionSlot(build,entity,i,&slot);
        xls_ir_input_proof* const* proof=slot<build->baseline->inputs.Count ? xrtArrayConstGet(&build->baseline->inputs,slot) : NULL;
        xls_typed_input_identity* identity=xlsFrontendSemMemoIrIdentity(scope->semantic,scope->input,i);
        if(proof && xlsIrCapacityAdd(unit,*proof,identity,&nodes,&children))matched=1;
    }
    if(!matched || xlsFrontendCancelPoll())return;
    xlsIrCapacityReserve((void**)&module->nodes,&module->nodeCapacity,nodes,sizeof(*module->nodes));
    if(!xlsFrontendCancelPoll())xlsIrCapacityReserve((void**)&module->children,&module->childCapacity,children,sizeof(*module->children));
}
