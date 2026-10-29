#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_bound_nullable_generic_args.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"nullable_generic_callbacks"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t checked=0;
    for(size_t i=0;i<ir.instructionCount;++i){
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if(instruction->kind!=XL6_IR_INSTR_CALL || instruction->operandCount!=3)continue;
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(&ir,instruction->targetSymbol);
        const XL6_IrType* parameter=target && target->parameterCount==3
            ? xl6IrTypeGet(&ir.types,target->parameterTypes[1]) : NULL;
        const XL6_IrType* callable=parameter && parameter->kind==XL6_IR_TYPE_NULLABLE && parameter->paramCount==1
            ? xl6IrTypeGet(&ir.types,parameter->params[0]) : NULL;
        if(!callable || callable->kind!=XL6_IR_TYPE_FUNCTION)continue;
        // Closing the owner key cannot waive validation at the actual call.
        size_t position=instruction->firstOperand+1;
        XL6_IrValueId saved=ir.operands[position];
        ir.operands[position]=ir.operands[position+1];
        reject(&ir,"known generic callback cannot receive the scalar argument",0);
        ir.operands[position]=saved;
        assert(xl6IrVerifyModule(&ir,&parse.diags));++checked;
    }
    assert(checked>=8 && rejected==checked);
    puts("nullable generic callbacks: concrete argument ABI remains checked");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
