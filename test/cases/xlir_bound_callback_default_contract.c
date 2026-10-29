#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_callback_defaults.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"bound_callback_defaults"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t defaults=0,copied=0;
    for(size_t i=0;i<ir.instructionCount;++i)
        assert(ir.instructions[i].runtimeOp!=XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED);
    for(size_t c=0;c<ir.closureTargetCount;++c){
        const XL6_IrClosureTarget* closure=&ir.closureTargets[c];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        const XL6_IrNode* source=xl6IrNodeGet(&ir,closure->declaration);
        const XL6_IrNode* cast=source ? xl6IrNodeGet(&ir,source->parent) : NULL;
        const XL6_IrNode* parameter=cast ? xl6IrNodeGet(&ir,cast->parent) : NULL;
        if(!cast || cast->sourceKind!=XL6_AST_TYPE_AS || !parameter || parameter->sourceKind!=XL6_AST_PARAM)continue;
        assert(source->targetSymbol && closure->captureCount==1);
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(&ir,closure->adaptedTargetSymbol);
        const XL6_IrType* signature=xl6IrTypeGet(&ir.types,closure->signatureType);
        const XL6_IrFunction* body=NULL;
        for(size_t f=0;f<ir.functionCount;++f)
            if(ir.functions[f].symbol==closure->targetSymbol)body=&ir.functions[f];
        assert(target && signature && body);
        size_t expected=target->returnType!=signature->callableResultType,actual=0,calls=0;
        for(size_t i=body->firstInstruction;i<body->firstInstruction+body->instructionCount;++i){
            const XL6_IrInstruction* instruction=&ir.instructions[i];
            actual+=instruction->runtimeOp==XL6_IR_RUNTIME_CSTRING_COPY || instruction->runtimeOp==XL6_IR_RUNTIME_CBYTES_COPY;
            calls+=instruction->kind==XL6_IR_INSTR_CALL && instruction->targetSymbol==target->symbol;
        }
        assert(calls==1 && actual==expected);copied+=actual;
        XL6_SymbolId saved=source->targetSymbol;ir.nodes[closure->declaration-1].targetSymbol=0;
        reject(&ir,"default callback must retain its checked lexical selection",0);
        ir.nodes[closure->declaration-1].targetSymbol=saved;
        XL6_IrNodeId castId=(XL6_IrNodeId)(cast-ir.nodes+1);
        XL6_IrCastAbi abi=cast->castAbi;ir.nodes[castId-1].castAbi=XL6_IR_CAST_IDENTITY;
        reject(&ir,"default callback must retain its exact typed binding",0);
        ir.nodes[castId-1].castAbi=abi;
        assert(xl6IrVerifyModule(&ir,&parse.diags));++defaults;
    }
    assert(defaults>=9 && copied>=1 && rejected==defaults*2);
    puts("callback defaults: lexical selection, one receiver, exact binding and requested copies authenticated");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
