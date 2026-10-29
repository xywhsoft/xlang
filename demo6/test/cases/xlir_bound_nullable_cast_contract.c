#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_nullable_casts.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"nullable_method_casts"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t checked=0;
    for(size_t i=0;i<ir.instructionCount;++i){
        XL6_IrInstruction* instruction=&ir.instructions[i];
        const XL6_IrNode* origin=xl6IrNodeGet(&ir,instruction->origin);
        if(instruction->kind!=XL6_IR_INSTR_NULLABLE_WRAP ||
            instruction->castAbi!=XL6_IR_CAST_MANAGED_TO_POINTER_NULLABLE ||
            !origin || origin->sourceKind!=XL6_AST_TYPE_AS || instruction->operandCount!=1)continue;
        XL6_IrNodeId innerId=origin->childCount ? ir.children[origin->firstChild] : 0;
        const XL6_IrNode* inner=xl6IrNodeGet(&ir,innerId);
        if(!inner || inner->sourceKind!=XL6_AST_TYPE_AS || inner->childCount!=2)continue;
        const XL6_IrClosureTarget* closure=NULL;
        for(size_t c=0;c<ir.closureTargetCount;++c)
            if(ir.closureTargets[c].declaration==ir.children[inner->firstChild] &&
               ir.closureTargets[c].signatureType==inner->storageType)closure=&ir.closureTargets[c];
        if(!closure || closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        assert(closure->captureCount==1);
        size_t bindings=0;
        for(size_t b=0;b<ir.instructionCount;++b)
            bindings+=ir.instructions[b].kind==XL6_IR_INSTR_CLOSURE_MAKE &&
                ir.instructions[b].origin==innerId && ir.instructions[b].targetSymbol==closure->targetSymbol;
        assert(bindings==1);
        XL6_IrCastAbi saved=instruction->castAbi;instruction->castAbi=XL6_IR_CAST_IDENTITY;
        reject(&ir,"nullable method cast must retain its pointer lift",0);
        instruction->castAbi=saved;
        XL6_IrNode* source=&ir.nodes[closure->declaration-1];
        XL6_SymbolId selected=source->targetSymbol;source->targetSymbol=0;
        reject(&ir,"nullable method cast must retain its selected declaration",0);
        source->targetSymbol=selected;
        assert(xl6IrVerifyModule(&ir,&parse.diags));++checked;
    }
    assert(checked>=5 && rejected==checked*2);
    puts("nullable method casts: one binding, pointer lift and selection authenticated");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
