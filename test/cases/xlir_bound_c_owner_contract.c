#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
static size_t conversions(const XL6_IrModule* ir,XL6_IrTypeId fromId,XL6_IrTypeId toId)
{
    if(fromId==toId)return 0;
    const XL6_IrType* from=xl6IrTypeGet(&ir->types,fromId);
    const XL6_IrType* to=xl6IrTypeGet(&ir->types,toId);
    if(xl6IrTypeIsCOwned(from))return 1;
    assert(from && to && from->cAbi==XL6_IR_C_ABI_PRODUCT && to->cAbi==XL6_IR_C_ABI_PRODUCT);
    size_t count=0;
    for(size_t i=0;i<from->paramCount;++i)count+=conversions(ir,from->params[i],to->params[i]);
    return count;
}
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_c_owners.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"bound_c_owners"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t exact=0,copied=0;
    for(size_t c=0;c<ir.closureTargetCount;++c){
        XL6_IrClosureTarget* closure=&ir.closureTargets[c];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(&ir,closure->adaptedTargetSymbol);
        const XL6_IrType* signature=xl6IrTypeGet(&ir.types,closure->signatureType);
        const XL6_IrFunction* body=NULL;
        for(size_t f=0;f<ir.functionCount;++f)if(ir.functions[f].symbol==closure->targetSymbol)body=&ir.functions[f];
        assert(target && signature && body && closure->captureCount==1);
        size_t expected=conversions(&ir,target->returnType,signature->callableResultType),actual=0,calls=0;
        for(size_t i=body->firstInstruction;i<body->firstInstruction+body->instructionCount;++i){
            const XL6_IrInstruction* instruction=&ir.instructions[i];
            actual+=instruction->runtimeOp==XL6_IR_RUNTIME_CSTRING_COPY || instruction->runtimeOp==XL6_IR_RUNTIME_CBYTES_COPY;
            calls+=(instruction->kind==XL6_IR_INSTR_CALL && instruction->targetSymbol==target->symbol) ||
                instruction->kind==XL6_IR_INSTR_VIRTUAL_CALL;
            assert(instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED);
            if(instruction->runtimeOp==XL6_IR_RUNTIME_CSTRING_COPY || instruction->runtimeOp==XL6_IR_RUNTIME_CBYTES_COPY){
                XL6_IrRuntimeOp saved=instruction->runtimeOp;
                ir.instructions[i].runtimeOp=saved==XL6_IR_RUNTIME_CSTRING_COPY
                    ? XL6_IR_RUNTIME_CBYTES_COPY : XL6_IR_RUNTIME_CSTRING_COPY;
                reject(&ir,"text and binary result-copy providers cannot be interchanged",0);
                ir.instructions[i].runtimeOp=saved;
                assert(xl6IrVerifyModule(&ir,&parse.diags));
            }
        }
        assert(calls==1 && actual==expected);
        if(expected)++copied;else ++exact;
    }
    assert(exact>=6 && copied>=5 && rejected>=5);
    puts("bound C owners: one receiver, one call, exact per-result copy plan");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
