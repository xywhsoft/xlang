#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_compound.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"bound_compound"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t checked=0,bindings=0;
    for(size_t i=0;i<ir.instructionCount;++i)
        assert(ir.instructions[i].runtimeOp!=XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED);
    for(size_t c=0;c<ir.closureTargetCount;++c){
        const XL6_IrClosureTarget* closure=&ir.closureTargets[c];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(&ir,closure->adaptedTargetSymbol);
        const XL6_IrType* signature=xl6IrTypeGet(&ir.types,closure->signatureType);
        const XL6_IrFunction* body=NULL;
        for(size_t f=0;f<ir.functionCount;++f)
            if(ir.functions[f].symbol==closure->targetSymbol)body=&ir.functions[f];
        assert(target && signature && body && closure->captureCount==1);
        assert(target->returnType==signature->callableResultType);
        size_t calls=0;
        for(size_t i=body->firstInstruction;i<body->firstInstruction+body->instructionCount;++i){
            const XL6_IrInstruction* instruction=&ir.instructions[i];
            assert(instruction->runtimeOp!=XL6_IR_RUNTIME_CSTRING_COPY &&
                instruction->runtimeOp!=XL6_IR_RUNTIME_CBYTES_COPY);
            calls+=instruction->kind==XL6_IR_INSTR_CALL && instruction->targetSymbol==target->symbol;
        }
        assert(calls==1);
        XL6_IrNode* source=&ir.nodes[closure->declaration-1];
        XL6_SymbolId saved=source->targetSymbol;source->targetSymbol=0;
        reject(&ir,"compound branch must authenticate its selected method",0);
        source->targetSymbol=saved;
        for(size_t i=0;i<ir.instructionCount;++i){
            XL6_IrInstruction* instruction=&ir.instructions[i];
            if(instruction->kind!=XL6_IR_INSTR_CLOSURE_MAKE || instruction->targetSymbol!=closure->targetSymbol)continue;
            assert(instruction->operandCount==1);
            instruction->operandCount=0;
            reject(&ir,"compound binding must capture its one selected receiver",0);
            instruction->operandCount=1;++bindings;
        }
        assert(xl6IrVerifyModule(&ir,&parse.diags));++checked;
    }
    assert(checked>=14 && bindings>=checked && rejected==checked+bindings);
    puts("compound methods: lazy selection, one receiver, exact result ABI, no checked projection");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
