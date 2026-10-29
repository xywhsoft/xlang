#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
static size_t resultCopies(const XL6_IrModule* ir,XL6_IrTypeId fromId,XL6_IrTypeId toId)
{
    if(fromId==toId)return 0;
    const XL6_IrType* from=xl6IrTypeGet(&ir->types,fromId);
    const XL6_IrType* to=xl6IrTypeGet(&ir->types,toId);
    if(xl6IrTypeIsCOwned(from))return 1;
    assert(from && to && from->cAbi==XL6_IR_C_ABI_PRODUCT && to->cAbi==XL6_IR_C_ABI_PRODUCT);
    size_t count=0;
    for(size_t i=0;i<from->paramCount;++i)count+=resultCopies(ir,from->params[i],to->params[i]);
    return count;
}
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_variadic_owners.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"bound_variadic_owners"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t arrays=0,dicts=0,exact=0,copied=0,shortened=0;
    for(size_t c=0;c<ir.closureTargetCount;++c){
        XL6_IrClosureTarget* closure=&ir.closureTargets[c];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        XL6_IrCallTarget* target=NULL;
        for(size_t t=0;t<ir.callTargetCount;++t)
            if(ir.callTargets[t].symbol==closure->adaptedTargetSymbol)target=&ir.callTargets[t];
        const XL6_IrType* signature=xl6IrTypeGet(&ir.types,closure->signatureType);
        if(!target || target->variadicAbi==XL6_IR_CALL_VARIADIC_NONE)continue;
        const XL6_IrFunction* body=NULL;
        for(size_t f=0;f<ir.functionCount;++f)
            if(ir.functions[f].symbol==closure->targetSymbol)body=&ir.functions[f];
        assert(signature && body && closure->captureCount==1 && signature->paramCount);
        arrays+=target->variadicAbi==XL6_IR_CALL_VARIADIC_ARRAY;
        dicts+=target->variadicAbi==XL6_IR_CALL_VARIADIC_DICT;
        shortened+=target->parameterCount>signature->paramCount+1;
        size_t expected=resultCopies(&ir,target->returnType,signature->callableResultType),actual=0,calls=0;
        XL6_SymbolId incoming=ir.functionParameters[body->firstParameter+body->parameterCount-1];
        for(size_t i=body->firstInstruction;i<body->firstInstruction+body->instructionCount;++i){
            XL6_IrInstruction* instruction=&ir.instructions[i];
            actual+=instruction->runtimeOp==XL6_IR_RUNTIME_CSTRING_COPY || instruction->runtimeOp==XL6_IR_RUNTIME_CBYTES_COPY;
            assert(instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED);
            assert(instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ARRAY_NEW);
            if(!((instruction->kind==XL6_IR_INSTR_CALL && instruction->targetSymbol==target->symbol) ||
                 instruction->kind==XL6_IR_INSTR_VIRTUAL_CALL))continue;
            ++calls;
            assert(instruction->operandCount==target->parameterCount);
            size_t tail=instruction->firstOperand+instruction->operandCount-1;
            const XL6_IrInstruction* pack=definition(&ir,ir.operands[tail]);
            assert(pack && pack->kind==XL6_IR_INSTR_LOAD && pack->targetSymbol==incoming);
            // Changing the physical tail to self must fail before either C
            // implementation or header emission can publish an invalid call.
            XL6_IrValueId saved=ir.operands[tail];ir.operands[tail]=ir.operands[instruction->firstOperand];
            reject(&ir,"bound variadic call must borrow its typed incoming pack",0);
            ir.operands[tail]=saved;
        }
        assert(calls==1 && actual==expected);
        if(expected)++copied;else ++exact;
        XL6_IrCallVariadicAbi saved=target->variadicAbi;
        target->variadicAbi=XL6_IR_CALL_VARIADIC_NONE;
        reject(&ir,"bound method must authenticate its pack erasure",0);
        target->variadicAbi=saved;
        assert(xl6IrVerifyModule(&ir,&parse.diags));
    }
    assert(arrays>=5 && dicts>=4 && exact>=7 && copied>=3 && shortened>=6);
    assert(rejected==(arrays+dicts)*2);
    puts("bound variadic owners: one receiver, one call, borrowed final pack and exact copy plan");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
