#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
static bool constrained(const XL6_IrModule* ir,XL6_IrNodeId id)
{
    const XL6_IrNode* node=xl6IrNodeGet(ir,id);
    if(!node)return false;
    if(node->sourceKind==XL6_AST_GENERIC_CONSTRAINT && node->childCount)return true;
    for(size_t i=0;i<node->childCount;++i)
        if(constrained(ir,ir->children[node->firstChild+i]))return true;
    return false;
}
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_constraint_casts.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"bound_constraints"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t checked=0;
    for(size_t c=0;c<ir.closureTargetCount;++c){
        XL6_IrClosureTarget* closure=&ir.closureTargets[c];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        XL6_IrNode* source=&ir.nodes[closure->declaration-1];
        XL6_SymbolId selected=source->targetSymbol;
        const XL6_IrNode* declaration=xl6IrSymbolNodeGet(&ir,selected);
        if(!declaration || !constrained(&ir,(XL6_IrNodeId)(declaration-ir.nodes+1)))continue;
        source->targetSymbol=0;
        reject(&ir,"constraint selection evidence removed",0);
        source->targetSymbol=selected;
        assert(xl6IrVerifyModule(&ir,&parse.diags));++checked;
    }
    assert(checked>=2 && rejected>=checked);
    puts("bound constraints: missing constraint evidence rejected");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
