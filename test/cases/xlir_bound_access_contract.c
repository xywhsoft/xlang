#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_nonpublic.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"bound_access"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    for(size_t m=0;m<ir.dynamicMethodCount;++m){
        const XL6_IrNode* method=xl6IrNodeGet(&ir,ir.dynamicMethods[m].declaration);
        assert(method && !(method->flags&(XL6_AST_FLAG_PRIVATE|XL6_AST_FLAG_PROTECTED)));
    }
    size_t checked=0;
    for(size_t c=0;c<ir.closureTargetCount;++c){
        XL6_IrClosureTarget* closure=&ir.closureTargets[c];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(&ir,closure->adaptedTargetSymbol);
        const XL6_IrNode* method=target ? xl6IrNodeGet(&ir,target->declaration) : NULL;
        if(!method || !(method->flags&(XL6_AST_FLAG_PRIVATE|XL6_AST_FLAG_PROTECTED)))continue;
        XL6_IrNode* source=&ir.nodes[closure->declaration-1];
        XL6_IrNodeId parent=source->parent;source->parent=0;
        assert(!xl6IrBoundMethodTargetMatches(&ir,closure->declaration,target->symbol,closure->signatureType));
        reject(&ir,"nonpublic binding lost its lexical access context",0);
        source->parent=parent;
        XL6_SymbolId selected=source->targetSymbol;source->targetSymbol=0;
        assert(!xl6IrBoundMethodTargetMatches(&ir,closure->declaration,target->symbol,closure->signatureType));
        reject(&ir,"nonpublic binding lost its selected declaration",0);
        source->targetSymbol=selected;
        assert(xl6IrVerifyModule(&ir,&parse.diags));++checked;
    }
    assert(checked>=4 && rejected>=checked*2);
    puts("nonpublic bindings: access evidence checked, dynamic registry remains public");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
